/* ScreenCTR mock console — runs the console side of the protocol on a PC.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Uses the exact same protocol, pixel conversion and JPEG code as the 3DS
 * sysmodule, with synthetic 3DS framebuffers (RGB565 top, BGR8 bottom).
 * Lets the Windows app and the automated tests run without a console.
 *
 *   mock_console [--port 6464] [--input-port 6466] [--rosalina-port 4950]
 *                [--loss 0.0] [--auto-accept-ms 300] [--deny] [--fps 30]
 */
#define _DEFAULT_SOURCE
#include "screenctr_protocol.h"
#include "sc_jpeg.h"
#include "sc_pixfmt.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static uint64_t now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000; }
static int nonblock(int fd) { return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

static struct {
    int port, input_port, rosalina_port, fps, accept_ms, deny;
    double loss;
} opt = { SC_PORT_CONTROL, SC_PORT_INPUT, SC_ROSALINA_INPUT_PORT, 30, 300, 0, 0.0 };

enum { S_NONE, S_HELLO, S_AUTH, S_ACTIVE };
static struct {
    int st, fd; struct sockaddr_in addr; uint8_t rx[1024]; size_t rx_len;
    sc_hello hello; uint32_t token; sc_quality q; uint64_t t0, t_stats; int input_ok;
    uint32_t frames[2], bytes, skipped, enc_us[2];
} S = { .fd = -1 };
static int g_listen, g_input, g_relay, g_video;
static struct sockaddr_in g_rosalina;
static int g_remote_active; static uint32_t g_last_seq; static uint64_t g_last_input;
static volatile sig_atomic_t g_stop;

static void ctrl_send(uint8_t type, const uint8_t *p, uint16_t n)
{
    uint8_t buf[300]; size_t m = sc_write_ctrl(buf, sizeof buf, type, p, n);
    if (S.fd >= 0 && m) send(S.fd, buf, m, MSG_NOSIGNAL);
}

static void release_input(const char *why)
{
    uint8_t pkt[20]; sc_rosalina_neutral(pkt);
    sendto(g_relay, pkt, 20, 0, (struct sockaddr *)&g_rosalina, sizeof g_rosalina);
    if (g_remote_active) printf("[mock] input released (%s)\n", why);
    g_remote_active = 0;
}

static void session_end(const char *why)
{
    if (S.st == S_NONE) return;
    printf("[mock] session end: %s\n", why);
    if (S.fd >= 0) { ctrl_send(SC_MSG_BYE, NULL, 0); close(S.fd); }
    S.fd = -1; S.st = S_NONE; S.rx_len = 0;
    release_input("session end");
}

static void activate(int input_ok)
{
    S.token = (uint32_t)rand() ^ 0x5C7A0000u;
    S.input_ok = input_ok;
    sc_welcome w = { S.token, SC_PROTO_VERSION, (uint16_t)opt.input_port, (uint8_t)input_ok, 6 /* N2DSXL */ };
    uint8_t p[SC_WELCOME_SIZE]; sc_welcome_write(p, &w); ctrl_send(SC_MSG_WELCOME, p, sizeof p);
    S.q = (sc_quality){ 70, (uint8_t)opt.fps, 3, 0 };
    S.st = S_ACTIVE; S.t_stats = now_ms(); g_last_seq = 0; g_remote_active = 0;
    printf("[mock] session active with '%s' (input %s)\n", S.hello.name, input_ok ? "allowed" : "denied");
}

static void on_msg(uint8_t t, const uint8_t *p, uint16_t n)
{
    if (t == SC_MSG_HELLO && S.st == S_HELLO) {
        if (sc_hello_read(p, n, &S.hello) || S.hello.version != SC_PROTO_VERSION) {
            uint8_t r = SC_REJECT_VERSION; ctrl_send(SC_MSG_REJECT, &r, 1); session_end("bad hello"); return;
        }
        printf("[mock] HELLO from '%s', wants input=%d -> waiting for 'user' (%d ms)\n", S.hello.name, S.hello.flags & 1, opt.accept_ms);
        ctrl_send(SC_MSG_AUTH_PENDING, NULL, 0);
        S.st = S_AUTH; S.t0 = now_ms();
    } else if (t == SC_MSG_SET_QUALITY && S.st == S_ACTIVE) {
        sc_quality_read(p, n, &S.q); if (!S.q.screens) S.q.screens = 3;
        printf("[mock] quality=%u fps=%u screens=%u chroma=%u\n", S.q.quality, S.q.fps_cap, S.q.screens, S.q.chroma);
    } else if (t == SC_MSG_PING) ctrl_send(SC_MSG_PONG, p, n > 8 ? 8 : n);
    else if (t == SC_MSG_SET_INPUT && n) S.input_ok = S.input_ok && p[0];
    else if (t == SC_MSG_BYE) session_end("bye from PC");
}

/* Synthetic framebuffers in the console's native rotated layout */
static void make_frame(int screen, uint32_t t, uint8_t *rgb)
{
    int w = screen ? 320 : 400, h = 240;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = rgb + 3 * (y * w + x);
            if (screen == 0) {
                int bx = (int)((t * 4) % (unsigned)(w - 40));
                int in = x >= bx && x < bx + 40 && y >= 100 && y < 140;
                p[0] = in ? 255 : (uint8_t)(x * 255 / w); p[1] = in ? 255 : (uint8_t)(y * 255 / h); p[2] = in ? 255 : 60;
            } else { /* static bottom screen: a "touch me" grid */
                int g = (x % 40 == 0) || (y % 40 == 0);
                p[0] = g ? 200 : 30; p[1] = g ? 200 : 40; p[2] = g ? 220 : 90;
            }
        }
}

static uint8_t g_dgram[SC_VIDEO_MAX_DATAGRAM];
static void send_screen(int sc, uint32_t frame_id, const uint8_t *jpg, size_t n)
{
    struct sockaddr_in d = S.addr; d.sin_port = htons(S.hello.video_port);
    sc_video_hdr h = { SC_PROTO_VERSION, (uint8_t)sc, S.token, frame_id, 0, sc_fragment_count(n), (uint32_t)now_ms() };
    for (uint16_t i = 0; i < h.frag_count; i++) {
        size_t off = (size_t)i * SC_VIDEO_MAX_PAYLOAD, len = n - off; if (len > SC_VIDEO_MAX_PAYLOAD) len = SC_VIDEO_MAX_PAYLOAD;
        if (opt.loss > 0 && (double)rand() / RAND_MAX < opt.loss) continue; /* simulated Wi-Fi loss */
        h.frag_index = i; sc_video_hdr_write(g_dgram, &h); memcpy(g_dgram + SC_VIDEO_HDR_SIZE, jpg + off, len);
        sendto(g_video, g_dgram, SC_VIDEO_HDR_SIZE + len, 0, (struct sockaddr *)&d, sizeof d);
    }
    S.bytes += (uint32_t)n;
}

static void stream_tick(void)
{
    static const uint16_t repeat_ms[] = { 100, 300, 1000 };  /* same policy as console/source/stream.c */
    static uint8_t rgb[400 * 240 * 3], fb[400 * 240 * 4], back[400 * 240 * 3], jpg[2][SC_MAX_FRAME_BYTES];
    static size_t jlen[2]; static uint8_t rep[2];
    static uint32_t t, ids[2], last_hash[2]; static uint64_t last_sent[2];
    static sc_jpeg_encoder enc; static int cq = -1, cc = -1;
    if (cq != S.q.quality || cc != S.q.chroma) { cq = S.q.quality; cc = S.q.chroma; sc_jpeg_init(&enc, cq, cc ? SC_JPEG_444 : SC_JPEG_420); }
    t++;
    for (int sc = 0; sc < 2; sc++) {
        if (!(S.q.screens & (1 << sc))) continue;
        int w = sc ? 320 : 400, fmt = sc ? SC_FB_BGR8 : SC_FB_RGB565, stride = 240 * sc_fb_bpp(fmt);
        make_frame(sc, t, rgb);
        sc_rgb_to_fb(rgb, w, 240, fmt, stride, fb);                 /* what the GPU would hold */
        uint32_t hsh = sc_fb_hash(fb, stride, w, stride, 1);
        uint64_t now = now_ms();
        if (hsh == last_hash[sc] && jlen[sc]) {
            if (now - last_sent[sc] < repeat_ms[rep[sc]]) { S.skipped++; continue; }
            send_screen(sc, ++ids[sc], jpg[sc], jlen[sc]); last_sent[sc] = now; if (rep[sc] < 2) rep[sc]++;
            continue;
        }
        struct timespec a, b; clock_gettime(CLOCK_MONOTONIC, &a);
        sc_fb_to_rgb(fb, fmt, stride, w, 240, back);                /* what the sysmodule does */
        size_t n = sc_jpeg_encode_rgb(&enc, back, w, 240, w, jpg[sc], sizeof jpg[sc]);
        clock_gettime(CLOCK_MONOTONIC, &b);
        S.enc_us[sc] += (uint32_t)((b.tv_sec - a.tv_sec) * 1000000 + (b.tv_nsec - a.tv_nsec) / 1000);
        if (n) { jlen[sc] = n; send_screen(sc, ++ids[sc], jpg[sc], n); S.frames[sc]++; last_hash[sc] = hsh; last_sent[sc] = now; rep[sc] = 0; }
    }
}

static void input_poll(void)
{
    uint8_t buf[64]; struct sockaddr_in from; socklen_t fl = sizeof from;
    for (;;) {
        int n = (int)recvfrom(g_input, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) break;
        sc_input in;
        if (sc_input_read(buf, (size_t)n, &in) || S.st != S_ACTIVE || !S.input_ok || in.session_token != S.token) continue;
        if (g_remote_active && (int32_t)(in.seq - g_last_seq) <= 0) continue;
        g_last_seq = in.seq; g_last_input = now_ms(); g_remote_active = 1;
        uint8_t pkt[20]; sc_input_to_rosalina(&in, pkt);
        sendto(g_relay, pkt, 20, 0, (struct sockaddr *)&g_rosalina, sizeof g_rosalina);
    }
    if (g_remote_active && now_ms() - g_last_input > SC_INPUT_TIMEOUT_MS) release_input("timeout");
}

static int udp_bind(int port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0); struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind udp"); exit(1); }
    nonblock(fd); return fd;
}

static void on_sig(int s) { (void)s; g_stop = 1; }

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : "0";
        if (!strcmp(a, "--port")) { opt.port = atoi(v); i++; }
        else if (!strcmp(a, "--input-port")) { opt.input_port = atoi(v); i++; }
        else if (!strcmp(a, "--rosalina-port")) { opt.rosalina_port = atoi(v); i++; }
        else if (!strcmp(a, "--loss")) { opt.loss = atof(v); i++; }
        else if (!strcmp(a, "--fps")) { opt.fps = atoi(v); i++; }
        else if (!strcmp(a, "--auto-accept-ms")) { opt.accept_ms = atoi(v); i++; }
        else if (!strcmp(a, "--deny")) opt.deny = 1;
        else { fprintf(stderr, "unknown option %s\n", a); return 2; }
    }
    signal(SIGINT, on_sig); signal(SIGTERM, on_sig); signal(SIGPIPE, SIG_IGN);
    setvbuf(stdout, NULL, _IOLBF, 0);
    srand((unsigned)time(NULL));

    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1; setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a = { 0 }; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)opt.port); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(g_listen, (struct sockaddr *)&a, sizeof a) < 0 || listen(g_listen, 2) < 0) { perror("listen"); return 1; }
    nonblock(g_listen);
    g_input = udp_bind(opt.input_port);
    g_relay = socket(AF_INET, SOCK_DGRAM, 0);
    g_video = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&g_rosalina, 0, sizeof g_rosalina);
    g_rosalina.sin_family = AF_INET; g_rosalina.sin_port = htons((uint16_t)opt.rosalina_port); g_rosalina.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    printf("[mock] ScreenCTR mock console on TCP %d, input UDP %d, relay -> 127.0.0.1:%d, loss %.0f%%\n",
           opt.port, opt.input_port, opt.rosalina_port, opt.loss * 100);

    uint64_t next_frame = now_ms();
    while (!g_stop) {
        struct pollfd pf[3] = { { g_listen, POLLIN, 0 }, { g_input, POLLIN, 0 }, { S.fd, POLLIN, 0 } };
        poll(pf, S.fd >= 0 ? 3 : 2, 2);
        if (pf[0].revents & POLLIN) {
            struct sockaddr_in ca; socklen_t cl = sizeof ca;
            int fd = accept(g_listen, (struct sockaddr *)&ca, &cl);
            if (fd >= 0) {
                if (S.st != S_NONE) { uint8_t b[8], r = SC_REJECT_BUSY; size_t m = sc_write_ctrl(b, 8, SC_MSG_REJECT, &r, 1); send(fd, b, m, MSG_NOSIGNAL); close(fd); printf("[mock] rejected 2nd PC (busy)\n"); }
                else { nonblock(fd); int nd = 1; setsockopt(fd, IPPROTO_TCP, 1 /* TCP_NODELAY */, &nd, sizeof nd); S.fd = fd; S.addr = ca; S.st = S_HELLO; S.rx_len = 0; S.t0 = now_ms(); }
            }
        }
        input_poll();
        if (S.fd >= 0 && (pf[2].revents & (POLLIN | POLLHUP | POLLERR))) {
            ssize_t n = recv(S.fd, S.rx + S.rx_len, sizeof S.rx - S.rx_len, 0);
            if (n == 0 || (n < 0 && errno != EAGAIN)) session_end("tcp closed");
            else if (n > 0) {
                S.rx_len += (size_t)n;
                for (;;) {
                    uint8_t t; const uint8_t *p; uint16_t pl;
                    int used = sc_parse_ctrl(S.rx, S.rx_len, &t, &p, &pl);
                    if (used < 0) { session_end("malformed"); break; }
                    if (!used) break;
                    on_msg(t, p, pl);
                    if (S.fd < 0) break;
                    memmove(S.rx, S.rx + used, S.rx_len - (size_t)used); S.rx_len -= (size_t)used;
                }
            }
        }
        uint64_t t = now_ms();
        if (S.st == S_HELLO && t - S.t0 > 5000) session_end("hello timeout");
        if (S.st == S_AUTH && t - S.t0 >= (uint64_t)opt.accept_ms) {
            if (opt.deny) { uint8_t r = SC_REJECT_DENIED; ctrl_send(SC_MSG_REJECT, &r, 1); session_end("denied by user"); }
            else activate(S.hello.flags & SC_HELLO_WANTS_INPUT);
        }
        if (S.st == S_ACTIVE) {
            unsigned fps = S.q.fps_cap ? S.q.fps_cap : 60;
            if (t >= next_frame) { stream_tick(); next_frame = t + 1000 / fps; }
            if (t - S.t_stats >= 1000) {
                uint32_t ms = (uint32_t)(t - S.t_stats);
                sc_stats st = { { (uint16_t)(S.frames[0] * 10000 / ms), (uint16_t)(S.frames[1] * 10000 / ms) },
                                { (uint16_t)(S.frames[0] ? S.enc_us[0] / S.frames[0] : 0), (uint16_t)(S.frames[1] ? S.enc_us[1] / S.frames[1] : 0) },
                                (uint32_t)((uint64_t)S.bytes * 1000 / ms), (uint16_t)S.skipped, 0 };
                uint8_t p[SC_STATS_SIZE]; sc_stats_write(p, &st); ctrl_send(SC_MSG_STATS, p, sizeof p);
                memset(S.frames, 0, sizeof S.frames); memset(S.enc_us, 0, sizeof S.enc_us); S.bytes = S.skipped = 0; S.t_stats = t;
            }
        }
    }
    session_end("shutdown");
    printf("[mock] stopped\n");
    return 0;
}
