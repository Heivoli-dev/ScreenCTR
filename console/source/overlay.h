/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 * On-console menu drawn into the (paused) game's bottom framebuffer. */
#pragma once
#include <3ds.h>
#include <stdbool.h>

bool overlay_open(u32 owner_pid);   /* backs up the bottom screen */
void overlay_clear(u32 rgb);
void overlay_rect(int x, int y, int w, int h, u32 rgb);
/* UTF-8 text, 8x12 glyphs, scale 1 or 2. Returns the x after the text. */
int  overlay_text(int x, int y, const char *utf8, u32 rgb, int scale);
void overlay_present(void);
void overlay_close(void);           /* restores the original pixels */
bool overlay_is_open(void);
