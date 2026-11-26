/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "screenctr_protocol.h"
#include <string.h>

static void w16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void w32(uint8_t *p, uint32_t v) { w16(p, (uint16_t)v); w16(p + 2, (uint16_t)(v >> 16)); }
static uint16_t r16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t r32(const uint8_t *p) { return (uint32_t)r16(p) | ((uint32_t)r16(p + 2) << 16); }

/* ---------------- Control ---------------- */
size_t sc_write_ctrl(uint8_t *out, size_t cap, uint8_t type, const uint8_t *payload, uint16_t plen)
{
    if (plen > SC_MAX_CTRL_PAYLOAD || cap < (size_t)plen + 3) return 0;
    w16(out, (uint16_t)(plen + 1));
    out[2] = type;
    if (plen) memcpy(out + 3, payload, plen);
    return (size_t)plen + 3;
}

int sc_parse_ctrl(const uint8_t *in, size_t len, uint8_t *type, const uint8_t **payload, uint16_t *plen)
{
    if (len < 2) return 0;
    uint16_t l = r16(in);
    if (l < 1 || l > SC_MAX_CTRL_PAYLOAD + 1) return -1;
    if (len < (size_t)l + 2) return 0;
    *type = in[2];
    *payload = in + 3;
    *plen = (uint16_t)(l - 1);
    return (int)l + 2;
}

size_t sc_hello_write(uint8_t *o, const sc_hello *h)
{
    w32(o, h->magic); w16(o + 4, h->version); w16(o + 6, h->flags); w16(o + 8, h->video_port);
    memset(o + 10, 0, SC_NAME_LEN);
    size_t n = strlen(h->name); if (n > SC_NAME_LEN - 1) n = SC_NAME_LEN - 1;
    memcpy(o + 10, h->name, n);
    memcpy(o + 10 + SC_NAME_LEN, h->client_id, 16);
    return SC_HELLO_SIZE;
}
int sc_hello_read(const uint8_t *i, size_t len, sc_hello *h)
{
    if (len < SC_HELLO_SIZE) return -1;
    h->magic = r32(i); h->version = r16(i + 4); h->flags = r16(i + 6); h->video_port = r16(i + 8);
    memcpy(h->name, i + 10, SC_NAME_LEN); h->name[SC_NAME_LEN - 1] = 0;
    memcpy(h->client_id, i + 10 + SC_NAME_LEN, 16);
    return h->magic == SC_HELLO_MAGIC ? 0 : -1;
}
size_t sc_welcome_write(uint8_t *o, const sc_welcome *w)
{
    w32(o, w->session_token); w16(o + 4, w->version); w16(o + 6, w->input_port);
    o[8] = w->input_allowed; o[9] = w->console_model;
    return SC_WELCOME_SIZE;
}
int sc_welcome_read(const uint8_t *i, size_t len, sc_welcome *w)
{
    if (len < SC_WELCOME_SIZE) return -1;
    w->session_token = r32(i); w->version = r16(i + 4); w->input_port = r16(i + 6);
    w->input_allowed = i[8]; w->console_model = i[9];
    return 0;
}
size_t sc_quality_write(uint8_t *o, const sc_quality *q)
{ o[0] = q->quality; o[1] = q->fps_cap; o[2] = q->screens; o[3] = q->chroma; return SC_QUALITY_SIZE; }
int sc_quality_read(const uint8_t *i, size_t len, sc_quality *q)
{
    if (len < SC_QUALITY_SIZE) return -1;
    q->quality = i[0]; q->fps_cap = i[1]; q->screens = i[2]; q->chroma = i[3];
    if (q->quality < 10) q->quality = 10;
    if (q->quality > 95) q->quality = 95;
    if (q->chroma > 1) q->chroma = 0;
    q->screens &= 3;
    return 0;
}
size_t sc_ping_write(uint8_t *o, const sc_ping *p) { w32(o, p->seq); w32(o + 4, p->time_ms); return SC_PING_SIZE; }
int sc_ping_read(const uint8_t *i, size_t len, sc_ping *p)
{ if (len < SC_PING_SIZE) return -1; p->seq = r32(i); p->time_ms = r32(i + 4); return 0; }
size_t sc_stats_write(uint8_t *o, const sc_stats *s)
{
    w16(o, s->fps_x10[0]); w16(o + 2, s->fps_x10[1]); w16(o + 4, s->encode_us[0]); w16(o + 6, s->encode_us[1]);
    w32(o + 8, s->bytes_per_sec); w16(o + 12, s->frames_skipped); w16(o + 14, s->cpu_hint);
    return SC_STATS_SIZE;
}
int sc_stats_read(const uint8_t *i, size_t len, sc_stats *s)
{
    if (len < SC_STATS_SIZE) return -1;
    s->fps_x10[0] = r16(i); s->fps_x10[1] = r16(i + 2); s->encode_us[0] = r16(i + 4); s->encode_us[1] = r16(i + 6);
    s->bytes_per_sec = r32(i + 8); s->frames_skipped = r16(i + 12); s->cpu_hint = r16(i + 14);
    return 0;
}

/* ---------------- Video ---------------- */
size_t sc_video_hdr_write(uint8_t *o, const sc_video_hdr *h)
{
    w16(o, SC_VIDEO_MAGIC); o[2] = h->version; o[3] = h->screen;
    w32(o + 4, h->session_token); w32(o + 8, h->frame_id);
    w16(o + 12, h->frag_index); w16(o + 14, h->frag_count); w32(o + 16, h->capture_ms);
    return SC_VIDEO_HDR_SIZE;
}
int sc_video_hdr_read(const uint8_t *i, size_t len, sc_video_hdr *h)
{
    if (len < SC_VIDEO_HDR_SIZE || r16(i) != SC_VIDEO_MAGIC) return -1;
    h->version = i[2]; h->screen = i[3];
    h->session_token = r32(i + 4); h->frame_id = r32(i + 8);
    h->frag_index = r16(i + 12); h->frag_count = r16(i + 14); h->capture_ms = r32(i + 16);
    if (h->version != SC_PROTO_VERSION || h->screen >= SC_SCREEN_COUNT) return -1;
    if (h->frag_count == 0 || h->frag_count > SC_MAX_FRAGMENTS || h->frag_index >= h->frag_count) return -1;
    return 0;
}

/* ---------------- Input ---------------- */
size_t sc_input_write(uint8_t *o, const sc_input *in)
{
    w16(o, SC_INPUT_MAGIC); o[2] = SC_PROTO_VERSION; o[3] = in->flags;
    w32(o + 4, in->session_token); w32(o + 8, in->seq); w32(o + 12, in->buttons);
    w16(o + 16, (uint16_t)in->cpad_x); w16(o + 18, (uint16_t)in->cpad_y);
    w16(o + 20, (uint16_t)in->cstick_x); w16(o + 22, (uint16_t)in->cstick_y);
    w16(o + 24, in->touch_x); w16(o + 26, in->touch_y);
    return SC_INPUT_SIZE;
}
int sc_input_read(const uint8_t *b, size_t len, sc_input *in)
{
    if (len < SC_INPUT_SIZE || r16(b) != SC_INPUT_MAGIC || b[2] != SC_PROTO_VERSION) return -1;
    in->flags = b[3]; in->session_token = r32(b + 4); in->seq = r32(b + 8); in->buttons = r32(b + 12);
    in->cpad_x = (int16_t)r16(b + 16); in->cpad_y = (int16_t)r16(b + 18);
    in->cstick_x = (int16_t)r16(b + 20); in->cstick_y = (int16_t)r16(b + 22);
    in->touch_x = r16(b + 24); in->touch_y = r16(b + 26);
    if (in->touch_x > SC_BOTTOM_W - 1) in->touch_x = SC_BOTTOM_W - 1;
    if (in->touch_y > SC_BOTTOM_H - 1) in->touch_y = SC_BOTTOM_H - 1;
    return 0;
}
void sc_input_neutral(sc_input *in) { memset(in, 0, sizeof(*in)); }

/* Map -32767..32767 onto [center - range, center + range]. */
static uint32_t axis(int v, uint32_t center, uint32_t range)
{
    if (v < -32767) v = -32767;
    if (v > 32767) v = 32767;
    long r = (long)center + ((long)v * (long)range) / 32767L;
    return (uint32_t)r;
}

/* Rosalina InputRedirection packet (sysmodules/rosalina/source/input_redirection.c):
 *  [0..3]   HID pad, active LOW, bits 0..11 (neutral 0x00000FFF)
 *  [4..7]   touch: (1<<24)|(y<<12)|x with x,y in 0..0xFFF, neutral 0x02000000
 *  [8..11]  circle pad: x in bits 0..11, y in bits 12..23 (center 0x7FF), neutral 0x007FF7FF
 *  [12..15] IR / C-stick word, neutral 0x80800081:
 *           byte3 = C-stick Y, byte2 = C-stick X (center 0x80), byte1 = ZL/ZR bits, byte0 = 0x81
 *  [16..19] special buttons: bit0 HOME, bit1 POWER, bit2 POWER long
 * The layout of the circle pad / C-stick words follows the open source PC
 * clients written for Rosalina (e.g. InputClient-SDL); it MUST be confirmed on
 * hardware (see docs/STATUS.md). */
void sc_input_to_rosalina(const sc_input *in, uint8_t out[20])
{
    uint32_t pad = 0xFFFu & ~(in->buttons & 0xFFFu);
    uint32_t touch = 0x02000000u;
    if (in->flags & SC_INPUT_FLAG_TOUCH) {
        uint32_t tx = ((uint32_t)in->touch_x * 0xFFFu) / (SC_BOTTOM_W - 1);
        uint32_t ty = ((uint32_t)in->touch_y * 0xFFFu) / (SC_BOTTOM_H - 1);
        touch = (1u << 24) | (ty << 12) | tx;
    }
    /* Rosalina's neutral value is 0x7FF on both axes. */
    uint32_t cx = axis(in->cpad_x, 0x7FF, 0x7FF);
    uint32_t cy = axis(in->cpad_y, 0x7FF, 0x7FF);
    uint32_t circle = (cy << 12) | cx;

    uint32_t sx = axis(in->cstick_x, 0x80, 0x7F), sy = axis(in->cstick_y, 0x80, 0x7F);
    uint32_t zlzr = 0;
    if (in->buttons & SC_BTN_ZR) zlzr |= 0x02;
    if (in->buttons & SC_BTN_ZL) zlzr |= 0x04;
    uint32_t cpp = (sy << 24) | (sx << 16) | (zlzr << 8) | 0x81u;

    uint32_t special = 0;
    if (in->buttons & SC_BTN_HOME) special |= 1;
    if (in->buttons & SC_BTN_POWER) special |= 2;

    w32(out, pad); w32(out + 4, touch); w32(out + 8, circle); w32(out + 12, cpp); w32(out + 16, special);
}

void sc_rosalina_neutral(uint8_t out[20])
{
    w32(out, 0x00000FFFu); w32(out + 4, 0x02000000u); w32(out + 8, 0x007FF7FFu);
    w32(out + 12, 0x80800081u); w32(out + 16, 0);
}

/* ---------------- Fragmentation ---------------- */
uint16_t sc_fragment_count(size_t n)
{
    if (n == 0) return 1;
    return (uint16_t)((n + SC_VIDEO_MAX_PAYLOAD - 1) / SC_VIDEO_MAX_PAYLOAD);
}

void sc_reasm_init(sc_reassembler *r, uint8_t *buffer)
{
    memset(r, 0, sizeof(*r));
    r->buf = buffer;
}

static int newer(uint32_t a, uint32_t b) { return (int32_t)(a - b) > 0; }

int sc_reasm_feed(sc_reassembler *r, const sc_video_hdr *h, const uint8_t *payload, size_t plen)
{
    if (plen > SC_VIDEO_MAX_PAYLOAD) { r->fragments_rejected++; return 0; }
    if (h->frag_index + 1 < h->frag_count && plen != SC_VIDEO_MAX_PAYLOAD) { r->fragments_rejected++; return 0; }
    /* Late fragment of an already completed or abandoned frame */
    if (r->have_any_complete && !newer(h->frame_id, r->last_complete_id)) { r->fragments_rejected++; return 0; }

    if (!r->active || newer(h->frame_id, r->frame_id)) {
        if (r->active) r->frames_dropped++;
        r->active = 1;
        r->frame_id = h->frame_id;
        r->frag_count = h->frag_count;
        r->received = 0;
        r->size = 0;
        r->capture_ms = h->capture_ms;
        memset(r->got, 0, sizeof(r->got));
    } else if (h->frame_id != r->frame_id || h->frag_count != r->frag_count) {
        r->fragments_rejected++;
        return 0;
    }

    uint16_t i = h->frag_index;
    if (r->got[i >> 3] & (1u << (i & 7))) return 0; /* duplicate */
    memcpy(r->buf + (size_t)i * SC_VIDEO_MAX_PAYLOAD, payload, plen);
    r->got[i >> 3] |= (uint8_t)(1u << (i & 7));
    r->received++;
    if (i + 1 == h->frag_count) r->size = (uint32_t)i * SC_VIDEO_MAX_PAYLOAD + (uint32_t)plen;

    if (r->received == r->frag_count) {
        r->active = 0;
        r->have_any_complete = 1;
        r->last_complete_id = r->frame_id;
        r->frames_completed++;
        return 1;
    }
    return 0;
}
