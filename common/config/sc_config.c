/* ScreenCTR — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "sc_config.h"
#include "screenctr_protocol.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *name; uint32_t bit; } kButtons[] = {
    {"A", SC_BTN_A}, {"B", SC_BTN_B}, {"SELECT", SC_BTN_SELECT}, {"START", SC_BTN_START},
    {"RIGHT", SC_BTN_RIGHT}, {"LEFT", SC_BTN_LEFT}, {"UP", SC_BTN_UP}, {"DOWN", SC_BTN_DOWN},
    {"R", SC_BTN_R}, {"L", SC_BTN_L}, {"X", SC_BTN_X}, {"Y", SC_BTN_Y},
};

void sc_config_defaults(sc_config *c)
{
    memset(c, 0, sizeof(*c));
    c->hotkey = SC_BTN_L | SC_BTN_R | SC_BTN_START;
    c->remote_input = 1;
    c->auto_accept = 1;
    c->quality = 70;
    c->fps_cap = 30;
    c->chroma = 0;
    c->port = SC_PORT_CONTROL;
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) { if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return 0; a++; b++; }
    return *a == *b;
}

uint32_t sc_parse_buttons(const char *s)
{
    uint32_t m = 0; char tok[16];
    while (*s) {
        while (*s == ' ' || *s == '+') s++;
        size_t n = 0;
        while (*s && *s != '+' && *s != ' ' && n < sizeof(tok) - 1) tok[n++] = *s++;
        while (*s && *s != '+') { if (*s != ' ') return 0; s++; }
        if (!n) continue;
        tok[n] = 0;
        uint32_t bit = 0;
        for (size_t i = 0; i < sizeof(kButtons) / sizeof(kButtons[0]); i++) if (ieq(tok, kButtons[i].name)) bit = kButtons[i].bit;
        if (!bit) return 0;
        m |= bit;
    }
    return m;
}

size_t sc_format_buttons(uint32_t mask, char *out, size_t cap)
{
    size_t n = 0; if (cap) out[0] = 0;
    /* Display order: shoulders first, then the rest, as players read combos */
    static const int order[] = { 9, 8, 3, 2, 0, 1, 10, 11, 6, 7, 5, 4 };
    for (size_t k = 0; k < 12; k++) {
        int i = order[k];
        if (!(mask & kButtons[i].bit)) continue;
        int w = snprintf(out + n, cap > n ? cap - n : 0, "%s%s", n ? "+" : "", kButtons[i].name);
        if (w < 0) break;
        n += (size_t)w;
    }
    return n;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static int to_int(const char *v, int lo, int hi, int *out)
{
    char *end; long x = strtol(v, &end, 10);
    if (end == v || *trim(end) || x < lo || x > hi) return -1;
    *out = (int)x; return 0;
}

static int to_bool(const char *v, uint8_t *out)
{
    if (ieq(v, "1") || ieq(v, "true") || ieq(v, "yes") || ieq(v, "on") || ieq(v, "oui")) { *out = 1; return 0; }
    if (ieq(v, "0") || ieq(v, "false") || ieq(v, "no") || ieq(v, "off") || ieq(v, "non")) { *out = 0; return 0; }
    return -1;
}

int sc_config_parse(sc_config *c, const char *text)
{
    int bad = 0; char line[160];
    while (*text) {
        size_t n = 0;
        while (*text && *text != '\n') { if (n < sizeof(line) - 1) line[n++] = *text; text++; }
        if (*text) text++;
        line[n] = 0;
        char *l = trim(line);
        if (!*l || *l == '#' || *l == ';' || *l == '[') continue;
        char *eq = strchr(l, '=');
        if (!eq) { bad++; continue; }
        *eq = 0;
        char *k = trim(l), *v = trim(eq + 1);
        int iv, err = 0;
        if (ieq(k, "hotkey")) { uint32_t m = sc_parse_buttons(v); if (m) c->hotkey = m; else err = 1; }
        else if (ieq(k, "remote_input")) err = to_bool(v, &c->remote_input);
        else if (ieq(k, "auto_accept")) err = to_bool(v, &c->auto_accept);
        else if (ieq(k, "quality")) { err = to_int(v, 10, 95, &iv); if (!err) c->quality = (uint8_t)iv; }
        else if (ieq(k, "fps_cap")) { err = to_int(v, 0, 60, &iv); if (!err) c->fps_cap = (uint8_t)iv; }
        else if (ieq(k, "chroma")) { if (ieq(v, "420")) c->chroma = 0; else if (ieq(v, "444")) c->chroma = 1; else err = 1; }
        else if (ieq(k, "port")) { err = to_int(v, 1024, 65535, &iv); if (!err) c->port = (uint16_t)iv; }
        else if (ieq(k, "trusted")) {
            size_t vl = strlen(v);
            int inp = vl > 6 && ieq(v + vl - 6, "+input");
            if (inp) v[vl - 6] = 0;
            if (strlen(v) == 32) sc_config_trust(c, v, inp); else err = 1;
        }
        if (err) bad++;
    }
    return bad;
}

size_t sc_config_write(const sc_config *c, char *out, size_t cap)
{
    char hk[64]; sc_format_buttons(c->hotkey, hk, sizeof hk);
    int n = snprintf(out, cap,
        "# ScreenCTR — configuration console\n"
        "# Combinaison d'ouverture du menu (A B X Y L R START SELECT UP DOWN LEFT RIGHT)\n"
        "hotkey = %s\n"
        "# Contrôle de la console depuis le PC (on/off)\n"
        "remote_input = %s\n"
        "# Reconnexion des PC déjà autorisés sans confirmation\n"
        "auto_accept = %s\n"
        "quality = %u\nfps_cap = %u\nchroma = %s\nport = %u\n",
        hk, c->remote_input ? "on" : "off", c->auto_accept ? "on" : "off",
        c->quality, c->fps_cap, c->chroma ? "444" : "420", c->port);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t len = (size_t)n;
    for (int i = 0; i < c->trusted_count; i++) {
        int w = snprintf(out + len, cap - len, "trusted = %s\n", c->trusted[i]);
        if (w < 0 || (size_t)w >= cap - len) return 0;
        len += (size_t)w;
    }
    return len;
}

static int find_trusted(const sc_config *c, const char *id)
{
    for (int i = 0; i < c->trusted_count; i++) if (strncmp(c->trusted[i], id, 32) == 0) return i;
    return -1;
}

int sc_config_is_trusted(const sc_config *c, const char *id)
{
    if (strlen(id) != 32) return 0;
    int i = find_trusted(c, id);
    if (i < 0) return 0;
    return strcmp(c->trusted[i] + 32, "+input") == 0 ? 2 : 1;
}

void sc_config_trust(sc_config *c, const char *id, int allow_input)
{
    if (strlen(id) != 32) return;
    int i = find_trusted(c, id);
    if (i >= 0) { /* refresh: move to the end (most recent) */
        memmove(c->trusted[i], c->trusted[i + 1], sizeof(c->trusted[0]) * (size_t)(c->trusted_count - i - 1));
        c->trusted_count--;
    }
    if (c->trusted_count == SC_MAX_TRUSTED) { /* drop the oldest */
        memmove(c->trusted[0], c->trusted[1], sizeof(c->trusted[0]) * (SC_MAX_TRUSTED - 1));
        c->trusted_count--;
    }
    snprintf(c->trusted[c->trusted_count++], sizeof(c->trusted[0]), "%.32s%s", id, allow_input ? "+input" : "");
}

void sc_client_id_hex(const uint8_t id[16], char out[33])
{
    static const char hx[] = "0123456789abcdef";
    for (int i = 0; i < 16; i++) { out[2 * i] = hx[id[i] >> 4]; out[2 * i + 1] = hx[id[i] & 15]; }
    out[32] = 0;
}
