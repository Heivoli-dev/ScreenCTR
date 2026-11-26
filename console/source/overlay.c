/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "overlay.h"
#include "capture.h"
#include "sysutil.h"
#include "sc_pixfmt.h"
#include "font.h"
#include <stdlib.h>
#include <string.h>

#define OW 320
#define OH 240

static struct {
    bool open;
    fb_desc fb;
    u8 *ptr;        /* mapped framebuffer */
    u8 *backup;
    u8 *rgb;        /* OW*OH*3 canvas */
} g;

bool overlay_is_open(void) { return g.open; }

bool overlay_open(u32 owner)
{
    if (g.open) return true;
    fb_desc fb[2];
    if (R_FAILED(capture_query(fb))) return false;
    g.fb = fb[1];
    if (!sc_fb_bpp((int)g.fb.format)) return false;
    u32 size = g.fb.stride * g.fb.rows;
    g.backup = malloc(size);
    g.rgb = malloc(OW * OH * 3);
    g.ptr = g.backup && g.rgb ? capture_map_for_write(&g.fb, owner) : NULL;
    if (!g.ptr) { free(g.backup); free(g.rgb); g.backup = g.rgb = NULL; return false; }
    svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, g.ptr, size);
    memcpy(g.backup, g.ptr, size);
    /* Start from a dimmed copy of the game picture */
    sc_fb_to_rgb(g.ptr, (int)g.fb.format, (int)g.fb.stride, OW, OH, g.rgb);
    for (int i = 0; i < OW * OH * 3; i++) g.rgb[i] = (u8)(g.rgb[i] / 5);
    g.open = true;
    return true;
}

void overlay_clear(u32 c) { overlay_rect(0, 0, OW, OH, c); }

void overlay_rect(int x, int y, int w, int h, u32 c)
{
    if (!g.open) return;
    for (int j = y < 0 ? 0 : y; j < y + h && j < OH; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < OW; i++) {
            u8 *p = g.rgb + 3 * (j * OW + i);
            p[0] = (u8)(c >> 16); p[1] = (u8)(c >> 8); p[2] = (u8)c;
        }
}

static int glyph_index(u32 cp)
{
    for (int i = 0; i < FONT_COUNT; i++) if (font_cp[i] == cp) return i;
    return '?' - 32;
}

static u32 utf8_next(const char **s)
{
    const u8 *p = (const u8 *)*s; u32 c = *p++;
    if (c >= 0xC0 && c < 0xE0 && (*p & 0xC0) == 0x80) c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
    else if (c >= 0xE0 && c < 0xF0 && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) { c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F); p += 2; }
    *s = (const char *)p;
    return c;
}

int overlay_text(int x, int y, const char *s, u32 c, int scale)
{
    if (!g.open) return x;
    while (*s) {
        u32 cp = utf8_next(&s);
        const u8 *bits = font_bits[glyph_index(cp)];
        for (int j = 0; j < FONT_H; j++)
            for (int i = 0; i < FONT_W; i++)
                if (bits[j] & (0x80 >> i)) overlay_rect(x + i * scale, y + j * scale, scale, scale, c);
        x += FONT_W * scale;
    }
    return x;
}

void overlay_present(void)
{
    if (!g.open) return;
    sc_rgb_to_fb(g.rgb, OW, OH, (int)g.fb.format, (int)g.fb.stride, g.ptr);
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, g.ptr, g.fb.stride * g.fb.rows);
}

void overlay_close(void)
{
    if (!g.open) return;
    memcpy(g.ptr, g.backup, g.fb.stride * g.fb.rows);
    capture_unmap_for_write(&g.fb, g.ptr); /* flushes */
    free(g.backup); free(g.rgb);
    memset(&g, 0, sizeof(g));
}
