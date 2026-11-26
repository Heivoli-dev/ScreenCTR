/* ScreenCTR — unit tests for the protocol, pixel formats and JPEG encoder.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "screenctr_protocol.h"
#include "sc_pixfmt.h"
#include "sc_jpeg.h"
#include "sc_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures, checks;
#define CHECK(c) do { checks++; if (!(c)) { failures++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

static void test_ctrl(void)
{
    uint8_t buf[512], pl[64];
    sc_hello h = { SC_HELLO_MAGIC, SC_PROTO_VERSION, SC_HELLO_WANTS_INPUT, 6465, "PC-de-test", {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16} }, h2;
    size_t n = sc_hello_write(pl, &h);
    size_t m = sc_write_ctrl(buf, sizeof buf, SC_MSG_HELLO, pl, (uint16_t)n);
    CHECK(m == n + 3);
    uint8_t t; const uint8_t *p; uint16_t l;
    CHECK(sc_parse_ctrl(buf, 1, &t, &p, &l) == 0);          /* incomplete */
    CHECK(sc_parse_ctrl(buf, m - 1, &t, &p, &l) == 0);      /* incomplete */
    CHECK(sc_parse_ctrl(buf, m, &t, &p, &l) == (int)m);
    CHECK(t == SC_MSG_HELLO && l == SC_HELLO_SIZE);
    CHECK(sc_hello_read(p, l, &h2) == 0);
    CHECK(h2.video_port == 6465 && h2.flags == 1 && strcmp(h2.name, "PC-de-test") == 0 && h2.client_id[15] == 16);
    uint8_t bad[4] = { 0xFF, 0xFF, 1, 0 };
    CHECK(sc_parse_ctrl(bad, 4, &t, &p, &l) == -1);         /* oversized length */

    sc_quality q = { 200, 30, 0xFF, 7 }, q2;
    sc_quality_write(pl, &q); sc_quality_read(pl, 4, &q2);
    CHECK(q2.quality == 95 && q2.screens == 3 && q2.chroma == 0); /* clamped */

    sc_stats s = { {300, 150}, {12000, 9000}, 1234567, 42, 0 }, s2;
    sc_stats_write(pl, &s); CHECK(sc_stats_read(pl, SC_STATS_SIZE, &s2) == 0);
    CHECK(s2.fps_x10[1] == 150 && s2.bytes_per_sec == 1234567 && s2.encode_us[0] == 12000);
}

static void test_input(void)
{
    sc_input a, b; uint8_t buf[SC_INPUT_SIZE], r[20];
    sc_input_neutral(&a);
    a.session_token = 0xDEADBEEF; a.seq = 7; a.buttons = SC_BTN_A | SC_BTN_L | SC_BTN_ZR;
    a.cpad_x = -32767; a.cpad_y = 32767; a.cstick_x = 0; a.cstick_y = 0;
    a.flags = SC_INPUT_FLAG_TOUCH; a.touch_x = 319; a.touch_y = 0;
    CHECK(sc_input_write(buf, &a) == SC_INPUT_SIZE);
    CHECK(sc_input_read(buf, sizeof buf, &b) == 0);
    CHECK(b.session_token == 0xDEADBEEF && b.seq == 7 && b.buttons == a.buttons && b.cpad_x == -32767 && b.touch_x == 319);
    buf[0] ^= 1; CHECK(sc_input_read(buf, sizeof buf, &b) == -1);

    sc_input_to_rosalina(&a, r);
    CHECK(rd32(r) == (0xFFFu & ~(SC_BTN_A | SC_BTN_L)));   /* active low */
    CHECK(rd32(r + 4) == ((1u << 24) | (0u << 12) | 0xFFFu));
    CHECK((rd32(r + 8) & 0xFFF) == 0);                     /* full left */
    CHECK(((rd32(r + 8) >> 12) & 0xFFF) == 0xFFE);         /* full up   */
    CHECK(rd32(r + 12) == 0x80800281u);                    /* C-stick centred, ZR */

    /* Neutral state must equal Rosalina's own defaults exactly */
    uint8_t n1[20], n2[20]; sc_input z; sc_input_neutral(&z);
    sc_input_to_rosalina(&z, n1); sc_rosalina_neutral(n2);
    CHECK(memcmp(n1, n2, 20) == 0);
    CHECK(rd32(n2) == 0xFFF && rd32(n2 + 4) == 0x02000000 && rd32(n2 + 8) == 0x007FF7FF && rd32(n2 + 12) == 0x80800081);
}

static void test_reassembly(void)
{
    static uint8_t frame[50000], rb[SC_MAX_FRAME_BYTES];
    for (size_t i = 0; i < sizeof frame; i++) frame[i] = (uint8_t)(i * 31 + 7);
    sc_reassembler r; sc_reasm_init(&r, rb);
    uint16_t fc = sc_fragment_count(sizeof frame);
    CHECK(fc == 37);
    /* Feed fragments in reverse order with a duplicate */
    int done = 0;
    for (int k = fc - 1; k >= 0; k--) {
        sc_video_hdr h = { SC_PROTO_VERSION, 0, 1, 100, (uint16_t)k, fc, 5 };
        size_t off = (size_t)k * SC_VIDEO_MAX_PAYLOAD, len = sizeof frame - off;
        if (len > SC_VIDEO_MAX_PAYLOAD) len = SC_VIDEO_MAX_PAYLOAD;
        done += sc_reasm_feed(&r, &h, frame + off, len);
        if (k == 10) done += sc_reasm_feed(&r, &h, frame + off, len);
    }
    CHECK(done == 1 && r.size == sizeof frame && memcmp(rb, frame, sizeof frame) == 0);

    /* Frame 101 incomplete, then 102 complete -> 101 dropped, late 101 rejected */
    sc_video_hdr h1 = { SC_PROTO_VERSION, 0, 1, 101, 0, 2, 0 };
    CHECK(sc_reasm_feed(&r, &h1, frame, SC_VIDEO_MAX_PAYLOAD) == 0);
    sc_video_hdr h2 = { SC_PROTO_VERSION, 0, 1, 102, 0, 1, 0 };
    CHECK(sc_reasm_feed(&r, &h2, frame, 10) == 1 && r.size == 10 && r.frames_dropped == 1);
    h1.frag_index = 1;
    CHECK(sc_reasm_feed(&r, &h1, frame, 5) == 0 && r.fragments_rejected >= 1);
    /* Wrap-around of frame ids */
    sc_reasm_init(&r, rb);
    sc_video_hdr w1 = { SC_PROTO_VERSION, 0, 1, 0xFFFFFFFFu, 0, 1, 0 }, w2 = w1;
    w2.frame_id = 0;
    CHECK(sc_reasm_feed(&r, &w1, frame, 3) == 1 && sc_reasm_feed(&r, &w2, frame, 3) == 1);

    /* Header validation */
    uint8_t d[SC_VIDEO_HDR_SIZE]; sc_video_hdr hh = { SC_PROTO_VERSION, 1, 9, 9, 3, 2, 0 }, out;
    sc_video_hdr_write(d, &hh); CHECK(sc_video_hdr_read(d, sizeof d, &out) == -1); /* index >= count */
    hh.frag_index = 1; sc_video_hdr_write(d, &hh); CHECK(sc_video_hdr_read(d, sizeof d, &out) == 0 && out.screen == 1);
}

static void make_pattern(uint8_t *rgb, int w, int h, int t)
{
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t *p = rgb + 3 * (y * w + x);
            p[0] = (uint8_t)(x * 255 / (w - 1));
            p[1] = (uint8_t)(y * 255 / (h - 1));
            p[2] = (uint8_t)((((x + t) / 20 + y / 20) & 1) ? 230 : 30);
            if (x > w / 2 - 40 && x < w / 2 + 40 && y > h / 2 - 30 && y < h / 2 + 30) { p[0] = 255; p[1] = 255; p[2] = 255; }
        }
}

static void test_pixfmt(void)
{
    static uint8_t rgb[400 * 240 * 3], back[400 * 240 * 3], fb[400 * 240 * 4];
    make_pattern(rgb, 400, 240, 0);
    for (int f = 0; f <= 4; f++) {
        int bpp = sc_fb_bpp(f), stride = 240 * bpp;
        CHECK(sc_rgb_to_fb(rgb, 400, 240, f, stride, fb) == 0);
        CHECK(sc_fb_to_rgb(fb, f, stride, 400, 240, back) == 0);
        int maxerr = 0;
        for (size_t i = 0; i < sizeof rgb; i++) { int e = abs(rgb[i] - back[i]); if (e > maxerr) maxerr = e; }
        int tol = f <= 1 ? 0 : (f == SC_FB_RGBA4 ? 17 : 8);
        if (maxerr > tol) printf("  format %d maxerr %d\n", f, maxerr);
        CHECK(maxerr <= tol);
    }
    /* Orientation: memory row 0, first pixel is the screen's bottom-left corner */
    memset(fb, 0, sizeof fb);
    fb[0] = 0; fb[1] = 0; fb[2] = 255; /* BGR8: pure red at memory (0,0) */
    sc_fb_to_rgb(fb, SC_FB_BGR8, 720, 400, 240, back);
    const uint8_t *bl = back + 3 * (239 * 400 + 0);
    CHECK(bl[0] == 255 && bl[1] == 0 && bl[2] == 0);
    CHECK(sc_fb_to_rgb(fb, 9, 720, 400, 240, back) == -1);
    uint32_t h1 = sc_fb_hash(fb, 720, 400, 720, 1); fb[720 * 399 + 5] ^= 1;
    CHECK(h1 != sc_fb_hash(fb, 720, 400, 720, 1));
}

static void write_ppm(const char *path, const uint8_t *rgb, int w, int h)
{
    FILE *f = fopen(path, "wb"); fprintf(f, "P6\n%d %d\n255\n", w, h); fwrite(rgb, 3, (size_t)w * h, f); fclose(f);
}

static void test_jpeg(const char *outdir)
{
    static uint8_t rgb[400 * 240 * 3], out[SC_MAX_FRAME_BYTES];
    char path[512];
    make_pattern(rgb, 400, 240, 0);
    snprintf(path, sizeof path, "%s/ref_400x240.ppm", outdir); write_ppm(path, rgb, 400, 240);
    int qs[] = { 30, 70, 90 };
    for (int c = 0; c < 2; c++)
        for (int qi = 0; qi < 3; qi++) {
            sc_jpeg_encoder e; sc_jpeg_init(&e, qs[qi], (sc_jpeg_chroma)c);
            size_t n = sc_jpeg_encode_rgb(&e, rgb, 400, 240, 400, out, sizeof out);
            CHECK(n > 0 && out[0] == 0xFF && out[1] == 0xD8 && out[n - 2] == 0xFF && out[n - 1] == 0xD9);
            printf("  jpeg q=%d %s: %zu bytes (%u fragments)\n", qs[qi], c ? "4:4:4" : "4:2:0", n, sc_fragment_count(n));
            snprintf(path, sizeof path, "%s/enc_q%d_%s.jpg", outdir, qs[qi], c ? "444" : "420");
            FILE *f = fopen(path, "wb"); fwrite(out, 1, n, f); fclose(f);
        }
    /* Bottom screen size + overflow handling */
    sc_jpeg_encoder e; sc_jpeg_init(&e, 70, SC_JPEG_420);
    make_pattern(rgb, 320, 240, 3);
    snprintf(path, sizeof path, "%s/ref_320x240.ppm", outdir); write_ppm(path, rgb, 320, 240);
    size_t n = sc_jpeg_encode_rgb(&e, rgb, 320, 240, 320, out, sizeof out);
    snprintf(path, sizeof path, "%s/enc_bottom_q70_420.jpg", outdir);
    FILE *f = fopen(path, "wb"); fwrite(out, 1, n, f); fclose(f);
    CHECK(sc_jpeg_encode_rgb(&e, rgb, 320, 240, 320, out, 100) == 0);
    /* Odd size (edge clamping) */
    make_pattern(rgb, 37, 21, 0);
    snprintf(path, sizeof path, "%s/ref_37x21.ppm", outdir); write_ppm(path, rgb, 37, 21);
    n = sc_jpeg_encode_rgb(&e, rgb, 37, 21, 37, out, sizeof out);
    snprintf(path, sizeof path, "%s/enc_odd_q70_420.jpg", outdir);
    f = fopen(path, "wb"); fwrite(out, 1, n, f); fclose(f);

    /* Host speed (indicative only — NOT representative of the ARM11) */
    static uint8_t fbuf[400 * 240 * 3];
    make_pattern(rgb, 400, 240, 0);
    clock_t t0 = clock(); int iters = 200;
    for (int i = 0; i < iters; i++) {
        sc_fb_to_rgb(rgb, SC_FB_BGR8, 720, 400, 240, fbuf);
        sc_jpeg_encode_rgb(&e, fbuf, 400, 240, 400, out, sizeof out);
    }
    double ms = 1000.0 * (double)(clock() - t0) / CLOCKS_PER_SEC / iters;
    printf("  host: convert+encode 400x240 q70 = %.2f ms/frame\n", ms);
}

static void test_config(void)
{
    sc_config c; sc_config_defaults(&c);
    CHECK(c.hotkey == (SC_BTN_L | SC_BTN_R | SC_BTN_START));
    CHECK(sc_parse_buttons("l + r+ Select") == (SC_BTN_L | SC_BTN_R | SC_BTN_SELECT));
    CHECK(sc_parse_buttons("L+ZL") == 0);      /* ZL is not readable from the HID register */
    CHECK(sc_parse_buttons("L R") == 0);
    char buf[1024];
    sc_format_buttons(SC_BTN_START | SC_BTN_R | SC_BTN_L, buf, sizeof buf);
    CHECK(strcmp(buf, "L+R+START") == 0);
    int bad = sc_config_parse(&c, "# comment\n[screenctr]\nhotkey = L+DOWN+X\r\nquality=200\nremote_input = off\n"
                                  "chroma=444\ngarbage\ntrusted = 00112233445566778899aabbccddeeff\ntrusted = ffffffffffffffffffffffffffffffff+input\n");
    CHECK(bad == 2);                           /* quality out of range, 'garbage' */
    CHECK(c.hotkey == (SC_BTN_L | SC_BTN_DOWN | SC_BTN_X) && c.quality == 70 && c.remote_input == 0 && c.chroma == 1);
    CHECK(sc_config_is_trusted(&c, "00112233445566778899aabbccddeeff") == 1);
    CHECK(sc_config_is_trusted(&c, "ffffffffffffffffffffffffffffffff") == 2);
    CHECK(sc_config_is_trusted(&c, "0011") == 0);
    uint8_t raw[16]; for (int i = 0; i < 16; i++) raw[i] = (uint8_t)(i * 17);
    char hex[33]; sc_client_id_hex(raw, hex); CHECK(strcmp(hex, "00112233445566778899aabbccddeeff") == 0);
    sc_config_trust(&c, hex, 1); CHECK(sc_config_is_trusted(&c, hex) == 2 && c.trusted_count == 2);
    for (int i = 0; i < 10; i++) { char id[40]; snprintf(id, sizeof id, "%032d", i); sc_config_trust(&c, id, 0); }
    CHECK(c.trusted_count == SC_MAX_TRUSTED && !sc_config_is_trusted(&c, "00112233445566778899aabbccddeeff"));
    size_t n = sc_config_write(&c, buf, sizeof buf);
    sc_config d; sc_config_defaults(&d);
    CHECK(n > 0 && sc_config_parse(&d, buf) == 0);
    CHECK(d.hotkey == c.hotkey && d.remote_input == 0 && d.trusted_count == SC_MAX_TRUSTED && d.chroma == 1);
}

int main(int argc, char **argv)
{
    test_config();
    const char *outdir = argc > 1 ? argv[1] : ".";
    test_ctrl(); test_input(); test_reassembly(); test_pixfmt(); test_jpeg(outdir);
    printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
