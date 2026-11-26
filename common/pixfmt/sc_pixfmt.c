/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "sc_pixfmt.h"

int sc_fb_bpp(int f)
{
    switch (f) {
    case SC_FB_RGBA8: return 4;
    case SC_FB_BGR8: return 3;
    case SC_FB_RGB565: case SC_FB_RGB5A1: case SC_FB_RGBA4: return 2;
    default: return 0;
    }
}

static inline uint8_t x5(unsigned v) { return (uint8_t)((v << 3) | (v >> 2)); }
static inline uint8_t x6(unsigned v) { return (uint8_t)((v << 2) | (v >> 4)); }
static inline uint8_t x4(unsigned v) { return (uint8_t)((v << 4) | v); }

int sc_fb_to_rgb(const uint8_t *fb, int format, int stride, int w, int h, uint8_t *rgb)
{
    int bpp = sc_fb_bpp(format);
    if (!bpp) return -1;
    /* Walk memory sequentially (cache friendly): memory row = screen column x,
     * memory pixel i = screen row (h-1-i). */
    for (int x = 0; x < w; x++) {
        const uint8_t *src = fb + (size_t)x * stride;
        uint8_t *dst = rgb + 3 * ((size_t)(h - 1) * w + x);
        const ptrdiff_t up = -(ptrdiff_t)(3 * w);
        switch (format) {
        case SC_FB_RGBA8:
            for (int i = 0; i < h; i++, src += 4, dst += up) { dst[0] = src[3]; dst[1] = src[2]; dst[2] = src[1]; }
            break;
        case SC_FB_BGR8:
            for (int i = 0; i < h; i++, src += 3, dst += up) { dst[0] = src[2]; dst[1] = src[1]; dst[2] = src[0]; }
            break;
        case SC_FB_RGB565:
            for (int i = 0; i < h; i++, src += 2, dst += up) {
                unsigned v = src[0] | (src[1] << 8);
                dst[0] = x5(v >> 11); dst[1] = x6((v >> 5) & 63); dst[2] = x5(v & 31);
            }
            break;
        case SC_FB_RGB5A1:
            for (int i = 0; i < h; i++, src += 2, dst += up) {
                unsigned v = src[0] | (src[1] << 8);
                dst[0] = x5(v >> 11); dst[1] = x5((v >> 6) & 31); dst[2] = x5((v >> 1) & 31);
            }
            break;
        case SC_FB_RGBA4:
            for (int i = 0; i < h; i++, src += 2, dst += up) {
                unsigned v = src[0] | (src[1] << 8);
                dst[0] = x4(v >> 12); dst[1] = x4((v >> 8) & 15); dst[2] = x4((v >> 4) & 15);
            }
            break;
        }
    }
    return 0;
}

int sc_rgb_to_fb(const uint8_t *rgb, int w, int h, int format, int stride, uint8_t *fb)
{
    int bpp = sc_fb_bpp(format);
    if (!bpp) return -1;
    for (int x = 0; x < w; x++) {
        uint8_t *d = fb + (size_t)x * stride;
        for (int i = 0; i < h; i++, d += bpp) {
            const uint8_t *s = rgb + 3 * ((size_t)(h - 1 - i) * w + x);
            unsigned r = s[0], g = s[1], b = s[2], v;
            switch (format) {
            case SC_FB_RGBA8: d[0] = 0xFF; d[1] = (uint8_t)b; d[2] = (uint8_t)g; d[3] = (uint8_t)r; break;
            case SC_FB_BGR8: d[0] = (uint8_t)b; d[1] = (uint8_t)g; d[2] = (uint8_t)r; break;
            case SC_FB_RGB565: v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3); d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); break;
            case SC_FB_RGB5A1: v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1; d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); break;
            case SC_FB_RGBA4: v = ((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | 15; d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); break;
            }
        }
    }
    return 0;
}

uint32_t sc_fb_hash(const uint8_t *fb, int stride, int rows, int row_bytes, int step)
{
    uint32_t h = 2166136261u;
    if (step < 1) step = 1;
    for (int r = 0; r < rows; r += step) {
        const uint8_t *p = fb + (size_t)r * stride;
        for (int i = 0; i + 4 <= row_bytes; i += 4) {
            uint32_t v = (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) | ((uint32_t)p[i + 2] << 16) | ((uint32_t)p[i + 3] << 24);
            h = (h ^ v) * 16777619u;
        }
    }
    return h;
}
