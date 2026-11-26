/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#pragma once
#include <3ds.h>
#include <stdbool.h>

typedef struct {
    u32 va;        /* address in the owner process (or VRAM)  */
    u32 format;    /* GSP pixel format (low 3 bits)            */
    u32 stride;    /* bytes per memory row (one screen column) */
    u32 rows;      /* 400 (top) or 320 (bottom)                */
    bool in_vram;
} fb_desc;

/* Query where both screens currently are. */
Result capture_query(fb_desc out[2]);

/* Copy a framebuffer into 'dst' (0x1000 aligned, >= stride*rows bytes) with
 * an inter-process DMA. 'owner_pid' is used when the framebuffer is not in VRAM. */
Result capture_copy(const fb_desc *fb, u32 owner_pid, void *dst, u32 dst_cap);

/* Map an application-owned framebuffer for writing (overlay). Returns a
 * pointer valid until capture_unmap_for_write(). VRAM is returned directly. */
void  *capture_map_for_write(const fb_desc *fb, u32 owner_pid);
void   capture_unmap_for_write(const fb_desc *fb, void *ptr);
