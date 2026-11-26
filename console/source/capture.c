/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Screen capture through GSPGPU_ImportDisplayCaptureInfo + svcStartInterProcessDma.
 * Same principle as HzMod / ChirunoMod (GPLv3). */
#include "capture.h"
#include "sysutil.h"

#define VRAM_BEGIN 0x1F000000u
#define VRAM_END   0x1F600000u
#define OVERLAY_MAP_VA 0x0A000000u /* free VA in our process for svcMapProcessMemoryEx */

Result capture_query(fb_desc out[2])
{
    GSPGPU_CaptureInfo ci;
    Result r = GSPGPU_ImportDisplayCaptureInfo(&ci);
    if (R_FAILED(r)) return r;
    for (int s = 0; s < 2; s++) {
        out[s].va = (u32)ci.screencapture[s].framebuf0_vaddr; /* left eye in 3D */
        out[s].format = ci.screencapture[s].format & 7;
        out[s].stride = ci.screencapture[s].framebuf_widthbytesize;
        out[s].rows = s == 0 ? 400 : 320;
        out[s].in_vram = out[s].va >= VRAM_BEGIN && out[s].va < VRAM_END;
    }
    return 0;
}

Result capture_copy(const fb_desc *fb, u32 owner_pid, void *dst, u32 dst_cap)
{
    u32 size = fb->stride * fb->rows;
    if (!fb->va || !size || size > dst_cap) return -1;

    Handle src = CUR_PROCESS_HANDLE, owned = 0;
    if (!fb->in_vram) {
        Result r = svcOpenProcess(&owned, owner_pid);
        if (R_FAILED(r)) return r;
        src = owned;
    }

    DmaConfig cfg;
    dmaConfigInitDefault(&cfg);
    Handle dma = 0;
    Result r = svcStartInterProcessDma(&dma, CUR_PROCESS_HANDLE, (u32)dst, src, fb->va, size, &cfg);
    if (R_SUCCEEDED(r)) {
        /* Poll until done (≈ 0.3 ms for 384 KiB); give up after 20 ms. */
        u64 t0 = now_ms();
        DmaState st = DMASTATE_STARTING;
        while (R_SUCCEEDED(svcGetDmaState(&st, dma)) && st != DMASTATE_DONE) {
            if (now_ms() - t0 > 20) { svcStopDma(dma); r = -2; break; }
            svcSleepThread(50 * 1000);
        }
        svcCloseHandle(dma);
    }
    if (owned) svcCloseHandle(owned);
    if (R_SUCCEEDED(r)) svcInvalidateProcessDataCache(CUR_PROCESS_HANDLE, dst, size);
    return r;
}

void *capture_map_for_write(const fb_desc *fb, u32 owner_pid)
{
    u32 size = fb->stride * fb->rows;
    if (fb->in_vram) return (void *)fb->va; /* VRAM is mapped in our process (RSF) */
    Handle h;
    if (R_FAILED(svcOpenProcess(&h, owner_pid))) return NULL;
    u32 base = fb->va & ~0xFFFu, off = fb->va - base;
    u32 len = (off + size + 0xFFF) & ~0xFFFu;
    Result r = svcMapProcessMemoryEx(CUR_PROCESS_HANDLE, OVERLAY_MAP_VA, h, base, len, 0);
    svcCloseHandle(h);
    if (R_FAILED(r)) { sc_log("map overlay failed %08lX", r); return NULL; }
    return (void *)(OVERLAY_MAP_VA + off);
}

void capture_unmap_for_write(const fb_desc *fb, void *ptr)
{
    u32 size = fb->stride * fb->rows;
    svcFlushProcessDataCache(CUR_PROCESS_HANDLE, ptr, size);
    if (fb->in_vram) return;
    u32 off = fb->va & 0xFFF;
    u32 len = (off + size + 0xFFF) & ~0xFFFu;
    svcUnmapProcessMemoryEx(CUR_PROCESS_HANDLE, OVERLAY_MAP_VA, len);
}
