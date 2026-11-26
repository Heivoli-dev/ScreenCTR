/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Minimal baseline JPEG encoder (JFIF, Huffman tables from ITU T.81 Annex K,
 * integer LL&M DCT). Written for the ARM11 of the 3DS: no FPU, no malloc,
 * no global state, so one encoder per thread is safe. */
#ifndef SC_JPEG_H
#define SC_JPEG_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SC_JPEG_420 = 0, SC_JPEG_444 = 1 } sc_jpeg_chroma;

typedef struct {
    uint16_t qy[64], qc[64];      /* quantizers in natural order, pre-scaled x8 */
    int quality;
    sc_jpeg_chroma chroma;
    uint8_t dqt_y[64], dqt_c[64]; /* zig-zag order, as written in the file */
} sc_jpeg_encoder;

void   sc_jpeg_init(sc_jpeg_encoder *e, int quality, sc_jpeg_chroma chroma);

/* Encode an upright RGB888 image (rgb[3*(y*stride_px + x)]).
 * Returns the JPEG size in bytes, or 0 if 'cap' was too small. */
size_t sc_jpeg_encode_rgb(const sc_jpeg_encoder *e, const uint8_t *rgb, int w, int h, int stride_px,
                          uint8_t *out, size_t cap);

#ifdef __cplusplus
}
#endif
#endif
