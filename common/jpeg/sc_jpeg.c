/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "sc_jpeg.h"
#include <string.h>

static const uint8_t kZigzag[64] = {
     0, 1, 8,16, 9, 2, 3,10, 17,24,32,25,18,11, 4, 5,
    12,19,26,33,40,48,41,34, 27,20,13, 6, 7,14,21,28,
    35,42,49,56,57,50,43,36, 29,22,15,23,30,37,44,51,
    58,59,52,45,38,31,39,46, 53,60,61,54,47,55,62,63 };

static const uint8_t kStdLum[64] = {
    16,11,10,16,24,40,51,61, 12,12,14,19,26,58,60,55, 14,13,16,24,40,57,69,56,
    14,17,22,29,51,87,80,62, 18,22,37,56,68,109,103,77, 24,35,55,64,81,104,113,92,
    49,64,78,87,103,121,120,101, 72,92,95,98,112,100,103,99 };
static const uint8_t kStdChr[64] = {
    17,18,24,47,99,99,99,99, 18,21,26,66,99,99,99,99, 24,26,56,99,99,99,99,99,
    47,66,99,99,99,99,99,99, 99,99,99,99,99,99,99,99, 99,99,99,99,99,99,99,99,
    99,99,99,99,99,99,99,99, 99,99,99,99,99,99,99,99 };

/* Annex K Huffman tables */
static const uint8_t kDcLumBits[16] = {0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0};
static const uint8_t kDcLumVal[12]  = {0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8_t kDcChrBits[16] = {0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0};
static const uint8_t kDcChrVal[12]  = {0,1,2,3,4,5,6,7,8,9,10,11};
static const uint8_t kAcLumBits[16] = {0,2,1,3,3,2,4,3,5,5,4,4,0,0,1,0x7d};
static const uint8_t kAcLumVal[162] = {
    0x01,0x02,0x03,0x00,0x04,0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,
    0x22,0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,0x52,0xd1,0xf0,
    0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,0x18,0x19,0x1a,0x25,0x26,0x27,0x28,
    0x29,0x2a,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,
    0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,0x69,
    0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x83,0x84,0x85,0x86,0x87,0x88,0x89,
    0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
    0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,0xc4,0xc5,
    0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,
    0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa };
static const uint8_t kAcChrBits[16] = {0,2,1,2,4,4,3,4,7,5,4,4,0,1,2,0x77};
static const uint8_t kAcChrVal[162] = {
    0x00,0x01,0x02,0x03,0x11,0x04,0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,
    0x13,0x22,0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,0x52,0xf0,
    0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,0xf1,0x17,0x18,0x19,0x1a,0x26,
    0x27,0x28,0x29,0x2a,0x35,0x36,0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,
    0x49,0x4a,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,0x67,0x68,
    0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x82,0x83,0x84,0x85,0x86,0x87,
    0x88,0x89,0x8a,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,
    0xa6,0xa7,0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,0xc3,
    0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,
    0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,
    0xf9,0xfa };

typedef struct { uint16_t code[256]; uint8_t size[256]; } huff_t;

static void build_huff(huff_t *t, const uint8_t bits[16], const uint8_t *vals)
{
    memset(t, 0, sizeof(*t));
    uint16_t code = 0; int k = 0;
    for (int len = 1; len <= 16; len++) {
        for (int i = 0; i < bits[len - 1]; i++) {
            t->code[vals[k]] = code; t->size[vals[k]] = (uint8_t)len; k++; code++;
        }
        code <<= 1;
    }
}

/* Tables are constant; build once lazily (idempotent, race is harmless
 * since every thread writes identical values). */
static huff_t gDcY, gDcC, gAcY, gAcC;
static volatile int gHuffReady;
static void ensure_huff(void)
{
    if (gHuffReady) return;
    build_huff(&gDcY, kDcLumBits, kDcLumVal); build_huff(&gDcC, kDcChrBits, kDcChrVal);
    build_huff(&gAcY, kAcLumBits, kAcLumVal); build_huff(&gAcC, kAcChrBits, kAcChrVal);
    gHuffReady = 1;
}

void sc_jpeg_init(sc_jpeg_encoder *e, int quality, sc_jpeg_chroma chroma)
{
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;
    int scale = quality < 50 ? 5000 / quality : 200 - quality * 2;
    for (int i = 0; i < 64; i++) {
        int y = (kStdLum[i] * scale + 50) / 100, c = (kStdChr[i] * scale + 50) / 100;
        if (y < 1) y = 1;
        if (y > 255) y = 255;
        if (c < 1) c = 1;
        if (c > 255) c = 255;
        e->qy[i] = (uint16_t)(y * 8); e->qc[i] = (uint16_t)(c * 8);
    }
    for (int i = 0; i < 64; i++) { e->dqt_y[i] = (uint8_t)(e->qy[kZigzag[i]] / 8); e->dqt_c[i] = (uint8_t)(e->qc[kZigzag[i]] / 8); }
    e->quality = quality; e->chroma = chroma;
    ensure_huff();
}

/* ---------------- Forward DCT (LL&M, as in IJG jfdctint "islow") ---------------- */
#define CONST_BITS 13
#define PASS1_BITS 2
#define F_0_298 2446
#define F_0_390 3196
#define F_0_541 4433
#define F_0_765 6270
#define F_0_899 7373
#define F_1_175 9633
#define F_1_501 12299
#define F_1_847 15137
#define F_1_961 16069
#define F_2_053 16819
#define F_2_562 20995
#define F_3_072 25172
#define DESCALE(x, n) (((x) + (1 << ((n) - 1))) >> (n))

static void fdct(int32_t *d)
{
    int32_t t0,t1,t2,t3,t4,t5,t6,t7,t10,t11,t12,t13,z1,z2,z3,z4,z5;
    int32_t *p = d;
    for (int r = 0; r < 8; r++, p += 8) {
        t0 = p[0] + p[7]; t7 = p[0] - p[7]; t1 = p[1] + p[6]; t6 = p[1] - p[6];
        t2 = p[2] + p[5]; t5 = p[2] - p[5]; t3 = p[3] + p[4]; t4 = p[3] - p[4];
        t10 = t0 + t3; t13 = t0 - t3; t11 = t1 + t2; t12 = t1 - t2;
        p[0] = (t10 + t11) * (1 << PASS1_BITS); p[4] = (t10 - t11) * (1 << PASS1_BITS);
        z1 = (t12 + t13) * F_0_541;
        p[2] = DESCALE(z1 + t13 * F_0_765, CONST_BITS - PASS1_BITS);
        p[6] = DESCALE(z1 - t12 * F_1_847, CONST_BITS - PASS1_BITS);
        z1 = t4 + t7; z2 = t5 + t6; z3 = t4 + t6; z4 = t5 + t7;
        z5 = (z3 + z4) * F_1_175;
        t4 *= F_0_298; t5 *= F_2_053; t6 *= F_3_072; t7 *= F_1_501;
        z1 *= -F_0_899; z2 *= -F_2_562; z3 *= -F_1_961; z4 *= -F_0_390;
        z3 += z5; z4 += z5;
        p[7] = DESCALE(t4 + z1 + z3, CONST_BITS - PASS1_BITS);
        p[5] = DESCALE(t5 + z2 + z4, CONST_BITS - PASS1_BITS);
        p[3] = DESCALE(t6 + z2 + z3, CONST_BITS - PASS1_BITS);
        p[1] = DESCALE(t7 + z1 + z4, CONST_BITS - PASS1_BITS);
    }
    p = d;
    for (int c = 0; c < 8; c++, p++) {
        t0 = p[0] + p[56]; t7 = p[0] - p[56]; t1 = p[8] + p[48]; t6 = p[8] - p[48];
        t2 = p[16] + p[40]; t5 = p[16] - p[40]; t3 = p[24] + p[32]; t4 = p[24] - p[32];
        t10 = t0 + t3; t13 = t0 - t3; t11 = t1 + t2; t12 = t1 - t2;
        p[0] = DESCALE(t10 + t11, PASS1_BITS); p[32] = DESCALE(t10 - t11, PASS1_BITS);
        z1 = (t12 + t13) * F_0_541;
        p[16] = DESCALE(z1 + t13 * F_0_765, CONST_BITS + PASS1_BITS);
        p[48] = DESCALE(z1 - t12 * F_1_847, CONST_BITS + PASS1_BITS);
        z1 = t4 + t7; z2 = t5 + t6; z3 = t4 + t6; z4 = t5 + t7;
        z5 = (z3 + z4) * F_1_175;
        t4 *= F_0_298; t5 *= F_2_053; t6 *= F_3_072; t7 *= F_1_501;
        z1 *= -F_0_899; z2 *= -F_2_562; z3 *= -F_1_961; z4 *= -F_0_390;
        z3 += z5; z4 += z5;
        p[56] = DESCALE(t4 + z1 + z3, CONST_BITS + PASS1_BITS);
        p[40] = DESCALE(t5 + z2 + z4, CONST_BITS + PASS1_BITS);
        p[24] = DESCALE(t6 + z2 + z3, CONST_BITS + PASS1_BITS);
        p[8]  = DESCALE(t7 + z1 + z4, CONST_BITS + PASS1_BITS);
    }
}

/* ---------------- Bit writer ---------------- */
typedef struct { uint8_t *p, *end; uint32_t acc; int n; int overflow; } bw_t;

static void put_byte(bw_t *b, uint8_t v)
{
    if (b->p >= b->end) { b->overflow = 1; return; }
    *b->p++ = v;
}
static void put_bits(bw_t *b, uint32_t code, int size)
{
    b->acc = (b->acc << size) | (code & ((1u << size) - 1));
    b->n += size;
    while (b->n >= 8) {
        uint8_t c = (uint8_t)(b->acc >> (b->n - 8));
        put_byte(b, c);
        if (c == 0xFF) put_byte(b, 0);
        b->n -= 8;
    }
}
static void flush_bits(bw_t *b) { if (b->n > 0) put_bits(b, 0x7F, 8 - b->n); }

static int nbits(int v) { if (v < 0) v = -v; int n = 0; while (v) { n++; v >>= 1; } return n; }

static int encode_block(bw_t *b, int32_t *blk, const uint16_t *q, int prev_dc, const huff_t *dc, const huff_t *ac)
{
    fdct(blk);
    int32_t zz[64];
    for (int i = 0; i < 64; i++) {
        int k = kZigzag[i];
        int32_t v = blk[k], d = q[k];
        zz[i] = v >= 0 ? (v + d / 2) / d : -((-v + d / 2) / d);
    }
    int diff = zz[0] - prev_dc, s = nbits(diff);
    put_bits(b, dc->code[s], dc->size[s]);
    if (s) put_bits(b, (uint32_t)(diff < 0 ? diff - 1 : diff), s);
    int run = 0;
    for (int i = 1; i < 64; i++) {
        int v = zz[i];
        if (!v) { run++; continue; }
        while (run >= 16) { put_bits(b, ac->code[0xF0], ac->size[0xF0]); run -= 16; }
        s = nbits(v);
        int sym = (run << 4) | s;
        put_bits(b, ac->code[sym], ac->size[sym]);
        put_bits(b, (uint32_t)(v < 0 ? v - 1 : v), s);
        run = 0;
    }
    if (run) put_bits(b, ac->code[0], ac->size[0]);
    return zz[0];
}

/* ---------------- Headers ---------------- */
static void put16(bw_t *b, int v) { put_byte(b, (uint8_t)(v >> 8)); put_byte(b, (uint8_t)v); }
static void put_dht(bw_t *b, int cls_id, const uint8_t bits[16], const uint8_t *vals)
{
    int n = 0; for (int i = 0; i < 16; i++) n += bits[i];
    put16(b, 0xFFC4); put16(b, 2 + 1 + 16 + n); put_byte(b, (uint8_t)cls_id);
    for (int i = 0; i < 16; i++) put_byte(b, bits[i]);
    for (int i = 0; i < n; i++) put_byte(b, vals[i]);
}

static void write_headers(bw_t *b, const sc_jpeg_encoder *e, int w, int h)
{
    static const uint8_t app0[] = {0xFF,0xE0,0,16,'J','F','I','F',0,1,1,0,0,1,0,1,0,0};
    put16(b, 0xFFD8);
    for (size_t i = 0; i < sizeof(app0); i++) put_byte(b, app0[i]);
    put16(b, 0xFFDB); put16(b, 2 + 65 * 2);
    put_byte(b, 0); for (int i = 0; i < 64; i++) put_byte(b, e->dqt_y[i]);
    put_byte(b, 1); for (int i = 0; i < 64; i++) put_byte(b, e->dqt_c[i]);
    put16(b, 0xFFC0); put16(b, 8 + 3 * 3); put_byte(b, 8); put16(b, h); put16(b, w); put_byte(b, 3);
    put_byte(b, 1); put_byte(b, e->chroma == SC_JPEG_420 ? 0x22 : 0x11); put_byte(b, 0);
    put_byte(b, 2); put_byte(b, 0x11); put_byte(b, 1);
    put_byte(b, 3); put_byte(b, 0x11); put_byte(b, 1);
    put_dht(b, 0x00, kDcLumBits, kDcLumVal); put_dht(b, 0x10, kAcLumBits, kAcLumVal);
    put_dht(b, 0x01, kDcChrBits, kDcChrVal); put_dht(b, 0x11, kAcChrBits, kAcChrVal);
    put16(b, 0xFFDA); put16(b, 6 + 2 * 3); put_byte(b, 3);
    put_byte(b, 1); put_byte(b, 0x00); put_byte(b, 2); put_byte(b, 0x11); put_byte(b, 3); put_byte(b, 0x11);
    put_byte(b, 0); put_byte(b, 63); put_byte(b, 0);
}

/* ---------------- Colour conversion ---------------- */
static inline void rgb2ycc(const uint8_t *px, int32_t *y, int32_t *cb, int32_t *cr)
{
    int r = px[0], g = px[1], bl = px[2];
    *y  = ((19595 * r + 38470 * g + 7471 * bl + 32768) >> 16) - 128;
    *cb = (-11059 * r - 21709 * g + 32768 * bl + 32768) >> 16;
    *cr = (32768 * r - 27439 * g - 5329 * bl + 32768) >> 16;
}

size_t sc_jpeg_encode_rgb(const sc_jpeg_encoder *e, const uint8_t *rgb, int w, int h, int stride,
                          uint8_t *out, size_t cap)
{
    bw_t b = { out, out + cap, 0, 0, 0 };
    write_headers(&b, e, w, h);
    int dy = 0, dcb = 0, dcr = 0;
    int32_t Y[4][64], CB[64], CR[64];
    const int mcu = e->chroma == SC_JPEG_420 ? 16 : 8;

    for (int my = 0; my < h; my += mcu) {
        for (int mx = 0; mx < w; mx += mcu) {
            if (mcu == 8) {
                for (int y = 0; y < 8; y++) {
                    int sy = my + y < h ? my + y : h - 1;
                    for (int x = 0; x < 8; x++) {
                        int sx = mx + x < w ? mx + x : w - 1;
                        rgb2ycc(rgb + 3 * (sy * stride + sx), &Y[0][y * 8 + x], &CB[y * 8 + x], &CR[y * 8 + x]);
                    }
                }
                dy = encode_block(&b, Y[0], e->qy, dy, &gDcY, &gAcY);
            } else {
                memset(CB, 0, sizeof(CB)); memset(CR, 0, sizeof(CR));
                for (int y = 0; y < 16; y++) {
                    int sy = my + y < h ? my + y : h - 1;
                    for (int x = 0; x < 16; x++) {
                        int sx = mx + x < w ? mx + x : w - 1;
                        int32_t yy, cb, cr;
                        rgb2ycc(rgb + 3 * (sy * stride + sx), &yy, &cb, &cr);
                        Y[(y >> 3) * 2 + (x >> 3)][(y & 7) * 8 + (x & 7)] = yy;
                        int ci = (y >> 1) * 8 + (x >> 1);
                        CB[ci] += cb; CR[ci] += cr;
                    }
                }
                for (int i = 0; i < 64; i++) { CB[i] = (CB[i] + 2) >> 2; CR[i] = (CR[i] + 2) >> 2; }
                for (int k = 0; k < 4; k++) dy = encode_block(&b, Y[k], e->qy, dy, &gDcY, &gAcY);
            }
            dcb = encode_block(&b, CB, e->qc, dcb, &gDcC, &gAcC);
            dcr = encode_block(&b, CR, e->qc, dcr, &gDcC, &gAcC);
            if (b.overflow) return 0;
        }
    }
    flush_bits(&b);
    put16(&b, 0xFFD9);
    return b.overflow ? 0 : (size_t)(b.p - out);
}
