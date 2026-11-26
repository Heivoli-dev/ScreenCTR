/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * 3DS framebuffer -> upright RGB888 conversion.
 * A 3DS framebuffer is stored rotated: each memory row ("stride" bytes) is
 * one screen COLUMN, from the bottom of the screen to the top. Formats are
 * the GSPGPU_FramebufferFormat values. */
#ifndef SC_PIXFMT_H
#define SC_PIXFMT_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

enum {
    SC_FB_RGBA8 = 0,  /* memory bytes: A B G R */
    SC_FB_BGR8 = 1,   /* memory bytes: B G R   */
    SC_FB_RGB565 = 2, /* u16 LE: RRRRRGGGGGGBBBBB */
    SC_FB_RGB5A1 = 3, /* u16 LE: RRRRRGGGGGBBBBBA */
    SC_FB_RGBA4 = 4   /* u16 LE: RRRRGGGGBBBBAAAA */
};

int sc_fb_bpp(int format); /* bytes per pixel, 0 if unknown */

/* Convert a rotated framebuffer of a w x h screen (w = 400 or 320, h = 240).
 * 'stride' is the byte length of one memory row (>= h * bpp).
 * Writes w*h*3 bytes. Returns 0 on success, -1 on bad format. */
int sc_fb_to_rgb(const uint8_t *fb, int format, int stride, int w, int h, uint8_t *rgb);

/* Inverse, used by the console overlay and by tests: upright RGB888 ->
 * rotated framebuffer. */
int sc_rgb_to_fb(const uint8_t *rgb, int w, int h, int format, int stride, uint8_t *fb);

/* Cheap change detector: hashes every 'step'-th memory row. */
uint32_t sc_fb_hash(const uint8_t *fb, int stride, int rows, int row_bytes, int step);

#ifdef __cplusplus
}
#endif
#endif
