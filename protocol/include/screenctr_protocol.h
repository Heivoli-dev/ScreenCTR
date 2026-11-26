/*
 * ScreenCTR — Two screens. One connection.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Shared wire protocol (console <-> PC). Portable C99, no OS dependency.
 * All multi-byte integers are little-endian on the wire. Structures are
 * serialized field by field (never memcpy'd) so the layout does not depend
 * on compiler packing. See protocol/PROTOCOL.md for the full specification.
 */
#ifndef SCREENCTR_PROTOCOL_H
#define SCREENCTR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SC_PROTO_VERSION        1u
#define SC_HELLO_MAGIC          0x52544353u /* "SCTR" */

#define SC_PORT_CONTROL         6464  /* TCP, console listens            */
#define SC_PORT_VIDEO           6465  /* UDP, PC listens (default)       */
#define SC_PORT_INPUT           6466  /* UDP, console listens            */
#define SC_ROSALINA_INPUT_PORT  4950  /* UDP, Luma3DS InputRedirection   */

/* ---------- Screens ---------- */
enum { SC_SCREEN_TOP = 0, SC_SCREEN_BOTTOM = 1, SC_SCREEN_COUNT = 2 };
#define SC_TOP_W     400
#define SC_TOP_H     240
#define SC_BOTTOM_W  320
#define SC_BOTTOM_H  240

/* ---------- Control channel (TCP) ----------
 * Each message: u16 len (bytes of type+payload), u8 type, payload. */
enum sc_msg_type {
    SC_MSG_HELLO        = 0x01, /* PC -> console */
    SC_MSG_AUTH_PENDING = 0x02, /* console -> PC: waiting for user on console */
    SC_MSG_WELCOME      = 0x03, /* console -> PC */
    SC_MSG_REJECT       = 0x04, /* console -> PC */
    SC_MSG_SET_QUALITY  = 0x10, /* PC -> console */
    SC_MSG_SET_INPUT    = 0x11, /* PC -> console */
    SC_MSG_PING         = 0x20, /* both ways */
    SC_MSG_PONG         = 0x21, /* both ways */
    SC_MSG_STATS        = 0x30, /* console -> PC, once per second */
    SC_MSG_BYE          = 0x7F  /* both ways */
};

enum sc_reject_reason {
    SC_REJECT_VERSION = 1, SC_REJECT_DENIED = 2, SC_REJECT_BUSY = 3, SC_REJECT_TIMEOUT = 4
};

#define SC_MAX_CTRL_PAYLOAD 256
#define SC_NAME_LEN 32

typedef struct {
    uint32_t magic;          /* SC_HELLO_MAGIC */
    uint16_t version;        /* SC_PROTO_VERSION */
    uint16_t flags;          /* bit0: wants remote input */
    uint16_t video_port;     /* UDP port the PC listens on */
    char     name[SC_NAME_LEN]; /* PC name, NUL padded (display only) */
    uint8_t  client_id[16];  /* random per PC install; used for trust */
} sc_hello;
#define SC_HELLO_SIZE (4 + 2 + 2 + 2 + SC_NAME_LEN + 16)
#define SC_HELLO_WANTS_INPUT 0x0001

typedef struct {
    uint32_t session_token;
    uint16_t version;
    uint16_t input_port;
    uint8_t  input_allowed;  /* user accepted remote control on the console */
    uint8_t  console_model;  /* CFG_SystemModel value */
} sc_welcome;
#define SC_WELCOME_SIZE (4 + 2 + 2 + 1 + 1)

typedef struct {
    uint8_t quality;   /* JPEG quality 10..95 */
    uint8_t fps_cap;   /* 0 = unlimited */
    uint8_t screens;   /* bit0 top, bit1 bottom */
    uint8_t chroma;    /* 0 = 4:2:0, 1 = 4:4:4 */
} sc_quality;
#define SC_QUALITY_SIZE 4

typedef struct { uint32_t seq; uint32_t time_ms; } sc_ping;
#define SC_PING_SIZE 8

typedef struct {
    uint16_t fps_x10[SC_SCREEN_COUNT];
    uint16_t encode_us[SC_SCREEN_COUNT]; /* average encode time per frame */
    uint32_t bytes_per_sec;
    uint16_t frames_skipped;             /* unchanged frames not sent */
    uint16_t cpu_hint;                   /* reserved */
} sc_stats;
#define SC_STATS_SIZE (4 + 4 + 4 + 2 + 2)

/* ---------- Video channel (UDP, console -> PC) ---------- */
#define SC_VIDEO_MAGIC        0x4353u  /* "SC" */
#define SC_VIDEO_HDR_SIZE     20
#define SC_VIDEO_MAX_PAYLOAD  1380
#define SC_VIDEO_MAX_DATAGRAM (SC_VIDEO_HDR_SIZE + SC_VIDEO_MAX_PAYLOAD)
#define SC_MAX_FRAME_BYTES    (192 * 1024)
#define SC_MAX_FRAGMENTS      ((SC_MAX_FRAME_BYTES + SC_VIDEO_MAX_PAYLOAD - 1) / SC_VIDEO_MAX_PAYLOAD)

typedef struct {
    uint8_t  version;
    uint8_t  screen;
    uint32_t session_token;
    uint32_t frame_id;
    uint16_t frag_index;
    uint16_t frag_count;
    uint32_t capture_ms;   /* console clock, for latency estimation */
} sc_video_hdr;

/* ---------- Input channel (UDP, PC -> console) ---------- */
#define SC_INPUT_MAGIC  0x4953u /* "SI" */
#define SC_INPUT_SIZE   28
#define SC_INPUT_TIMEOUT_MS 250 /* console releases everything after this */

/* ScreenCTR button bits (active high). Bits 0..11 match the 3DS HID PAD order. */
enum sc_button {
    SC_BTN_A = 1u << 0,  SC_BTN_B = 1u << 1,  SC_BTN_SELECT = 1u << 2, SC_BTN_START = 1u << 3,
    SC_BTN_RIGHT = 1u << 4, SC_BTN_LEFT = 1u << 5, SC_BTN_UP = 1u << 6, SC_BTN_DOWN = 1u << 7,
    SC_BTN_R = 1u << 8,  SC_BTN_L = 1u << 9,  SC_BTN_X = 1u << 10, SC_BTN_Y = 1u << 11,
    SC_BTN_ZL = 1u << 12, SC_BTN_ZR = 1u << 13, SC_BTN_HOME = 1u << 14, SC_BTN_POWER = 1u << 15
};
#define SC_INPUT_FLAG_TOUCH 0x01

typedef struct {
    uint8_t  flags;
    uint32_t session_token;
    uint32_t seq;
    uint32_t buttons;
    int16_t  cpad_x, cpad_y;     /* -32767..32767, +y = up */
    int16_t  cstick_x, cstick_y; /* -32767..32767, +y = up */
    uint16_t touch_x, touch_y;   /* 0..319, 0..239 */
} sc_input;

/* ---------- Serialization ---------- */
size_t sc_write_ctrl(uint8_t *out, size_t cap, uint8_t type, const uint8_t *payload, uint16_t plen);
/* Parse one control message from a stream buffer. Returns bytes consumed,
 * 0 if incomplete, -1 if malformed. */
int    sc_parse_ctrl(const uint8_t *in, size_t len, uint8_t *type, const uint8_t **payload, uint16_t *plen);

size_t sc_hello_write(uint8_t *out, const sc_hello *h);
int    sc_hello_read(const uint8_t *in, size_t len, sc_hello *h);
size_t sc_welcome_write(uint8_t *out, const sc_welcome *w);
int    sc_welcome_read(const uint8_t *in, size_t len, sc_welcome *w);
size_t sc_quality_write(uint8_t *out, const sc_quality *q);
int    sc_quality_read(const uint8_t *in, size_t len, sc_quality *q);
size_t sc_ping_write(uint8_t *out, const sc_ping *p);
int    sc_ping_read(const uint8_t *in, size_t len, sc_ping *p);
size_t sc_stats_write(uint8_t *out, const sc_stats *s);
int    sc_stats_read(const uint8_t *in, size_t len, sc_stats *s);

size_t sc_video_hdr_write(uint8_t *out, const sc_video_hdr *h);
int    sc_video_hdr_read(const uint8_t *in, size_t len, sc_video_hdr *h);

size_t sc_input_write(uint8_t *out, const sc_input *in);
int    sc_input_read(const uint8_t *buf, size_t len, sc_input *in);
void   sc_input_neutral(sc_input *in);

/* Convert a ScreenCTR input state to a Luma3DS Rosalina InputRedirection
 * 20-byte packet (UDP port 4950). */
void   sc_input_to_rosalina(const sc_input *in, uint8_t out[20]);
/* The neutral Rosalina packet: releases every remote input. */
void   sc_rosalina_neutral(uint8_t out[20]);

/* ---------- Frame fragmentation helpers ---------- */
uint16_t sc_fragment_count(size_t frame_bytes);

/* Reassembler: one per screen. Keeps only the newest frame in progress;
 * a newer frame_id discards an incomplete older one. Memory is caller-owned. */
typedef struct {
    uint8_t  *buf;              /* SC_MAX_FRAME_BYTES */
    uint32_t  frame_id;
    uint16_t  frag_count;
    uint16_t  received;
    uint32_t  size;             /* known once the last fragment arrives */
    uint32_t  capture_ms;
    uint8_t   active;
    uint8_t   have_any_complete;
    uint32_t  last_complete_id;
    uint8_t   got[(SC_MAX_FRAGMENTS + 7) / 8];
    /* statistics */
    uint32_t  frames_completed, frames_dropped, fragments_rejected;
} sc_reassembler;

void sc_reasm_init(sc_reassembler *r, uint8_t *buffer);
/* Feed one datagram payload (after header parse). Returns 1 when a frame is
 * complete (r->buf / r->size / r->frame_id valid until next call), else 0. */
int  sc_reasm_feed(sc_reassembler *r, const sc_video_hdr *h, const uint8_t *payload, size_t plen);

#ifdef __cplusplus
}
#endif
#endif
