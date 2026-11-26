/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Capture -> convert -> JPEG -> UDP fragments. Runs on core 2 of the New 3DS
 * (falls back to core 1). Unchanged screens are not re-sent, except once per
 * second so a client that lost packets resynchronises. */
#include "stream.h"
#include "capture.h"
#include "sysutil.h"
#include "sc_jpeg.h"
#include "sc_pixfmt.h"
#include <malloc.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define TOP_FB_CAP  (400 * 240 * 4)
#define BOT_FB_CAP  (320 * 240 * 4)
#define KEYFRAME_MS 1000
/* A static screen is re-sent (same JPEG, no re-encode) at +100 ms, +300 ms
 * and then every second: with Wi-Fi loss a single copy is often incomplete. */
static const u16 kRepeatMs[] = { 100, 300, KEYFRAME_MS };

static u8 *g_fb[2], *g_rgb, *g_jpeg[2];
static size_t g_jpeg_len[2];
static u8 g_dgram[SC_VIDEO_MAX_DATAGRAM];

bool stream_alloc(void)
{
    g_fb[0] = memalign(0x1000, TOP_FB_CAP);
    g_fb[1] = memalign(0x1000, BOT_FB_CAP);
    g_rgb = malloc(400 * 240 * 3);
    g_jpeg[0] = malloc(SC_MAX_FRAME_BYTES);
    g_jpeg[1] = malloc(SC_MAX_FRAME_BYTES);
    return g_fb[0] && g_fb[1] && g_rgb && g_jpeg[0] && g_jpeg[1];
}

static u32 ticks_to_us(u64 t) { return (u32)(t / (SYSCLOCK_ARM11 / 1000000)); }

static void send_frame(streamer *s, int screen, u32 frame_id, u32 cap_ms, const u8 *data, size_t n)
{
    sc_video_hdr h = { SC_PROTO_VERSION, (u8)screen, s->token, frame_id, 0, sc_fragment_count(n), cap_ms };
    for (u16 i = 0; i < h.frag_count; i++) {
        size_t off = (size_t)i * SC_VIDEO_MAX_PAYLOAD, len = n - off;
        if (len > SC_VIDEO_MAX_PAYLOAD) len = SC_VIDEO_MAX_PAYLOAD;
        h.frag_index = i;
        sc_video_hdr_write(g_dgram, &h);
        memcpy(g_dgram + SC_VIDEO_HDR_SIZE, data + off, len);
        sendto(s->sock, g_dgram, SC_VIDEO_HDR_SIZE + len, 0, (struct sockaddr *)&s->dst, sizeof(s->dst));
    }
    s->bytes += (u32)n;
}

static void stream_main(void *arg)
{
    streamer *s = arg;
    sc_jpeg_encoder enc;
    u8 cur_q = 0, cur_c = 0xFF;
    u32 frame_id[2] = { 0, 0 }, last_hash[2] = { 0, 0 };
    u64 last_sent[2] = { 0, 0 }, owner_checked = 0;
    u8 repeat[2] = { 0, 0 };
    u32 owner = 0;

    while (s->run) {
        u64 t_frame = now_ms();
        if (s->paused) { svcSleepThread(10 * 1000 * 1000); continue; }
        if (s->quality != cur_q || s->chroma != cur_c) {
            cur_q = s->quality; cur_c = s->chroma;
            sc_jpeg_init(&enc, cur_q, cur_c ? SC_JPEG_444 : SC_JPEG_420);
        }
        fb_desc fb[2];
        if (R_FAILED(capture_query(fb))) { svcSleepThread(5 * 1000 * 1000); continue; }
        if (t_frame - owner_checked > 500) { proc_foreground(&owner, NULL); owner_checked = t_frame; }

        for (int sc = 0; sc < 2 && s->run; sc++) {
            if (!(s->screens & (1 << sc))) continue;
            int bpp = sc_fb_bpp((int)fb[sc].format);
            if (!bpp || fb[sc].stride < 240u * bpp) continue;
            if (R_FAILED(capture_copy(&fb[sc], owner, g_fb[sc], sc ? BOT_FB_CAP : TOP_FB_CAP))) continue;
            u32 hsh = sc_fb_hash(g_fb[sc], (int)fb[sc].stride, (int)fb[sc].rows, 240 * bpp, 1);
            u64 now = now_ms();
            if (hsh == last_hash[sc] && g_jpeg_len[sc]) {
                if (now - last_sent[sc] < kRepeatMs[repeat[sc]]) { s->skipped++; continue; }
                send_frame(s, sc, ++frame_id[sc], (u32)now, g_jpeg[sc], g_jpeg_len[sc]); /* repeat, no re-encode */
                last_sent[sc] = now;
                if (repeat[sc] < 2) repeat[sc]++;
                continue;
            }
            u64 t0 = svcGetSystemTick();
            int w = sc ? SC_BOTTOM_W : SC_TOP_W;
            sc_fb_to_rgb(g_fb[sc], (int)fb[sc].format, (int)fb[sc].stride, w, 240, g_rgb);
            size_t n = sc_jpeg_encode_rgb(&enc, g_rgb, w, 240, w, g_jpeg[sc], SC_MAX_FRAME_BYTES);
            s->enc_us[sc] += ticks_to_us(svcGetSystemTick() - t0);
            if (!n) continue;
            g_jpeg_len[sc] = n;
            send_frame(s, sc, ++frame_id[sc], (u32)now, g_jpeg[sc], n);
            s->frames[sc]++;
            last_hash[sc] = hsh; last_sent[sc] = now; repeat[sc] = 0;
        }
        if (s->fps_cap) {
            u64 budget = 1000 / s->fps_cap, spent = now_ms() - t_frame;
            if (spent < budget) svcSleepThread((budget - spent) * 1000 * 1000);
        } else {
            svcSleepThread(1000 * 1000); /* always yield a little */
        }
    }
}

bool stream_start(streamer *s, u32 ip_be, u16 port, u32 token, const sc_quality *q)
{
    stream_stop(s);
    s->sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (s->sock < 0) return false;
    memset(&s->dst, 0, sizeof(s->dst));
    s->dst.sin_family = AF_INET; s->dst.sin_port = htons(port); s->dst.sin_addr.s_addr = ip_be;
    s->token = token;
    s->quality = q->quality; s->fps_cap = q->fps_cap; s->screens = q->screens; s->chroma = q->chroma;
    s->paused = false; s->run = true;
    s->frames[0] = s->frames[1] = s->enc_us[0] = s->enc_us[1] = s->bytes = s->skipped = 0;
    /* Core 2 exists on New 3DS only; ChirunoMod uses core 2 / prio 0x10 there. */
    s->core = 2;
    s->thread = threadCreate(stream_main, s, 0x4000, 0x10, 2, false);
    if (!s->thread) { s->core = 1; s->thread = threadCreate(stream_main, s, 0x4000, 0x30, 1, false); }
    if (!s->thread) { close(s->sock); s->sock = -1; s->run = false; return false; }
    sc_log("stream started on core %d -> %08lX:%u", s->core, ip_be, port);
    return true;
}

void stream_stop(streamer *s)
{
    if (!s->thread) return;
    s->run = false;
    threadJoin(s->thread, U64_MAX);
    threadFree(s->thread);
    s->thread = NULL;
    close(s->sock); s->sock = -1;
    sc_log("stream stopped");
}

void stream_take_stats(streamer *s, sc_stats *o, u32 ms)
{
    if (!ms) ms = 1;
    memset(o, 0, sizeof(*o));
    for (int i = 0; i < 2; i++) {
        u32 f = s->frames[i];
        o->fps_x10[i] = (u16)(f * 10000u / ms);
        o->encode_us[i] = (u16)(f ? (s->enc_us[i] / f > 65535 ? 65535 : s->enc_us[i] / f) : 0);
        s->frames[i] = 0; s->enc_us[i] = 0;
    }
    o->bytes_per_sec = (u32)((u64)s->bytes * 1000 / ms); s->bytes = 0;
    o->frames_skipped = (u16)(s->skipped > 65535 ? 65535 : s->skipped); s->skipped = 0;
}
