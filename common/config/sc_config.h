/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 * Console configuration (sd:/3ds/ScreenCTR/config.ini). Portable parser. */
#ifndef SC_CONFIG_H
#define SC_CONFIG_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SC_MAX_TRUSTED 8

typedef struct {
    uint32_t hotkey;          /* SC_BTN_* mask, physical HID buttons only (bits 0..11) */
    uint8_t  remote_input;    /* 0 = remote control fully disabled */
    uint8_t  auto_accept;     /* reconnect trusted PCs without confirmation */
    uint8_t  quality;         /* default JPEG quality */
    uint8_t  fps_cap;
    uint8_t  chroma;          /* 0 = 4:2:0, 1 = 4:4:4 */
    uint16_t port;            /* TCP control port */
    uint8_t  trusted_count;
    char     trusted[SC_MAX_TRUSTED][40]; /* "<hex client_id>" or "<hex client_id>+input" */
} sc_config;

void sc_config_defaults(sc_config *c);
/* Parse INI text (NUL terminated). Unknown keys are ignored. Returns the
 * number of lines that could not be understood. */
int  sc_config_parse(sc_config *c, const char *text);
size_t sc_config_write(const sc_config *c, char *out, size_t cap);

/* "L+R+START" -> mask. Returns 0 on any unknown or non-HID name. */
uint32_t sc_parse_buttons(const char *s);
size_t   sc_format_buttons(uint32_t mask, char *out, size_t cap);

/* 0 = unknown, 1 = trusted for streaming, 2 = trusted for streaming + control */
int  sc_config_is_trusted(const sc_config *c, const char *client_hex);
void sc_config_trust(sc_config *c, const char *client_hex, int allow_input);
void sc_client_id_hex(const uint8_t id[16], char out[33]);

#ifdef __cplusplus
}
#endif
#endif
