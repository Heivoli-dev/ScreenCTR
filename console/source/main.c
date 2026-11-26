/* ScreenCTR console sysmodule — Two screens. One connection.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later
 *
 * Threads:
 *   main   (core 1): hotkey, on-console menu, TCP session, remote input relay
 *   stream (core 2 on New 3DS): capture + JPEG + UDP video  (stream.c)
 */
#include <3ds.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "screenctr_protocol.h"
#include "sc_config.h"
#include "sysutil.h"
#include "stream.h"
#include "overlay.h"

#define SC_VERSION_STR "0.1.0"
#define SOC_BUF_SIZE   0x40000
#define HEAP_SIZE      0x2A0000  /* 2.6 MiB: see docs/STATUS.md (memory budget) */
#define AUTH_TIMEOUT_MS 30000

/* ------------------------------------------------------------------ */
/* libctru runtime overrides for a sysmodule (same pattern as Rosalina) */
/* ------------------------------------------------------------------ */
extern char *fake_heap_start, *fake_heap_end;
extern u32 __ctru_heap, __ctru_heap_size, __ctru_linear_heap, __ctru_linear_heap_size;

void __system_allocateHeaps(void)
{
    u32 tmp = 0;
    __ctru_heap_size = HEAP_SIZE;
    __ctru_heap = OS_HEAP_AREA_BEGIN;
    svcControlMemory(&tmp, __ctru_heap, 0, __ctru_heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE);
    __ctru_linear_heap = 0; __ctru_linear_heap_size = 0;
    fake_heap_start = (char *)__ctru_heap;
    fake_heap_end = fake_heap_start + __ctru_heap_size;
}

void __appInit(void)
{
    srvInit();
    hidInit();
    fsInit();
    archiveMountSdmc();
    gspInit();
    pmDbgInit();
    cfguInit();
}

void __appExit(void)
{
    cfguExit();
    pmDbgExit();
    gspExit();
    archiveUnmountAll();
    fsExit();
    hidExit();
    srvExit();
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */
typedef enum { S_NONE, S_HELLO, S_AUTH, S_ACTIVE } sess_state;
typedef enum { M_CLOSED, M_MAIN, M_AUTH, M_AUTH_INPUT } menu_state;

static sc_config g_cfg;
static streamer g_stream;

static struct {
    sess_state st;
    int fd;
    struct sockaddr_in addr;
    u8 rx[1024]; size_t rx_len;
    sc_hello hello;
    char id_hex[33];
    u32 token;
    bool input_allowed;     /* granted for this session */
    bool input_enabled;     /* PC side toggle (SET_INPUT) */
    sc_quality q;
    u64 t_start, t_stats;
} S = { .fd = -1 };

static struct {
    int listen_fd, input_fd, relay_fd;
    struct sockaddr_in rosalina;
    u32 last_seq; u64 last_input_ms; bool remote_active;
} N = { -1, -1, -1 };

static struct {
    menu_state st;
    u32 paused_pid; bool paused;
    u32 prev_keys;
    bool remember;
    bool dirty;
    char msg[64];
} M;

static bool g_quit;

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */
static void config_load(void)
{
    sc_config_defaults(&g_cfg);
    mkdir(SC_CFG_DIR, 0777);
    FILE *f = fopen(SC_CFG_PATH, "r");
    if (!f) { char buf[1024]; size_t n = sc_config_write(&g_cfg, buf, sizeof buf); f = fopen(SC_CFG_PATH, "w"); if (f) { fwrite(buf, 1, n, f); fclose(f); } return; }
    char *buf = calloc(1, 4096);
    if (buf) { fread(buf, 1, 4095, f); sc_config_parse(&g_cfg, buf); free(buf); }
    fclose(f);
}

static void config_save(void)
{
    char *buf = malloc(2048);
    if (!buf) return;
    size_t n = sc_config_write(&g_cfg, buf, 2048);
    FILE *f = fopen(SC_CFG_PATH, "w");
    if (f) { fwrite(buf, 1, n, f); fclose(f); }
    free(buf);
}

/* ------------------------------------------------------------------ */
/* Remote input                                                        */
/* ------------------------------------------------------------------ */
static void input_release(void)
{
    u8 pkt[20];
    sc_rosalina_neutral(pkt);
    if (N.relay_fd >= 0) sendto(N.relay_fd, pkt, sizeof pkt, 0, (struct sockaddr *)&N.rosalina, sizeof N.rosalina);
    if (N.remote_active) sc_log("remote input released");
    N.remote_active = false;
}

static bool input_permitted(void)
{
    return S.st == S_ACTIVE && g_cfg.remote_input && S.input_allowed && S.input_enabled && M.st == M_CLOSED;
}

static void input_poll(void)
{
    u8 buf[64];
    struct sockaddr_in from; socklen_t fl = sizeof from;
    for (int k = 0; k < 16; k++) {
        int n = recvfrom(N.input_fd, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
        if (n <= 0) break;
        sc_input in;
        if (sc_input_read(buf, (size_t)n, &in) != 0) continue;
        if (!input_permitted() || in.session_token != S.token) continue;
        if (from.sin_addr.s_addr != S.addr.sin_addr.s_addr) continue;      /* only the session PC */
        if (N.remote_active && (s32)(in.seq - N.last_seq) <= 0) continue; /* stale */
        N.last_seq = in.seq;
        N.last_input_ms = now_ms();
        N.remote_active = true;
        u8 pkt[20];
        sc_input_to_rosalina(&in, pkt);
        sendto(N.relay_fd, pkt, sizeof pkt, 0, (struct sockaddr *)&N.rosalina, sizeof N.rosalina);
    }
    /* Watchdog: nothing valid for SC_INPUT_TIMEOUT_MS, or permission lost */
    if (N.remote_active && (!input_permitted() || now_ms() - N.last_input_ms > SC_INPUT_TIMEOUT_MS)) input_release();
}

/* ------------------------------------------------------------------ */
/* Pause + menu                                                        */
/* ------------------------------------------------------------------ */
static void game_pause(void)
{
    if (M.paused) return;
    u32 pid;
    if (R_SUCCEEDED(proc_foreground(&pid, NULL)) && R_SUCCEEDED(proc_set_paused(pid, true))) { M.paused = true; M.paused_pid = pid; }
    else sc_log("pause failed");
}

static void game_resume(void)
{
    if (!M.paused) return;
    proc_set_paused(M.paused_pid, false);
    M.paused = false;
}

static void menu_open(menu_state st)
{
    if (M.st == M_CLOSED) {
        input_release();
        g_stream.paused = true;
        game_pause();
        if (!overlay_open(M.paused_pid)) {
            /* Never leave the game frozen behind an invisible menu. */
            sc_log("overlay unavailable (format/map): resuming game");
            game_resume();
        }
        M.prev_keys = hid_physical_buttons(); /* ignore keys already held */
    }
    M.st = st; M.dirty = true;
}

static void menu_close(void)
{
    if (M.st == M_CLOSED) return;
    overlay_close();
    game_resume();
    g_stream.paused = false;
    M.st = M_CLOSED;
}

static void ip_str(u32 be, char *out, size_t cap)
{
    u32 a = ntohl(be);
    snprintf(out, cap, "%lu.%lu.%lu.%lu", (a >> 24) & 255, (a >> 16) & 255, (a >> 8) & 255, a & 255);
}

#define C_BG     0x101218
#define C_ACCENT 0x3DDC97
#define C_TEXT   0xE8ECF2
#define C_DIM    0x8A93A6
#define C_WARN   0xFFB347

static void menu_draw(void)
{
    if (!overlay_is_open()) return;
    char line[96], ip[20], hk[48];
    overlay_rect(0, 0, 320, 240, C_BG);
    overlay_rect(0, 0, 320, 34, 0x1B1F2A);
    overlay_text(10, 5, "ScreenCTR", C_ACCENT, 2);
    overlay_text(170, 4, "Two screens.", C_DIM, 1);
    overlay_text(170, 17, "One connection.", C_DIM, 1);
    ip_str((u32)gethostid(), ip, sizeof ip);
    int y = 44;
    if (M.st == M_MAIN) {
        snprintf(line, sizeof line, "IP   : %s", ip);           overlay_text(12, y, line, C_TEXT, 1); y += 14;
        snprintf(line, sizeof line, "Port : %u", g_cfg.port);   overlay_text(12, y, line, C_TEXT, 1); y += 20;
        if (S.st == S_ACTIVE) snprintf(line, sizeof line, "Diffusion vers %.20s", S.hello.name);
        else snprintf(line, sizeof line, "En attente de l'application PC...");
        overlay_text(12, y, line, S.st == S_ACTIVE ? C_ACCENT : C_WARN, 1); y += 14;
        snprintf(line, sizeof line, "Contrôle à distance : %s", g_cfg.remote_input ? "activé" : "désactivé");
        overlay_text(12, y, line, C_TEXT, 1); y += 14;
        sc_format_buttons(g_cfg.hotkey, hk, sizeof hk);
        snprintf(line, sizeof line, "Menu : %s", hk);           overlay_text(12, y, line, C_DIM, 1); y += 14;
        overlay_text(12, y, "Rosalina > InputRedirection requis", C_DIM, 1); y += 22;
        overlay_text(12, y, "[B] Reprendre le jeu", C_TEXT, 1); y += 14;
        if (S.st == S_ACTIVE) { overlay_text(12, y, "[Y] Arrêter la diffusion", C_TEXT, 1); y += 14; }
        overlay_text(12, y, "[X] Contrôle à distance oui/non", C_TEXT, 1); y += 14;
        overlay_text(12, y, "[SELECT] Quitter ScreenCTR", C_TEXT, 1);
    } else if (M.st == M_AUTH) {
        char pip[20]; ip_str(S.addr.sin_addr.s_addr, pip, sizeof pip);
        overlay_text(12, y, "Demande de connexion", C_WARN, 1); y += 20;
        snprintf(line, sizeof line, "PC : %.28s", S.hello.name); overlay_text(12, y, line, C_TEXT, 1); y += 14;
        snprintf(line, sizeof line, "IP : %s", pip);               overlay_text(12, y, line, C_TEXT, 1); y += 24;
        overlay_text(12, y, "[A] Autoriser", C_ACCENT, 1); y += 14;
        overlay_text(12, y, "[Y] Autoriser et mémoriser ce PC", C_ACCENT, 1); y += 14;
        overlay_text(12, y, "[B] Refuser", C_TEXT, 1);
    } else if (M.st == M_AUTH_INPUT) {
        overlay_text(12, y, "Contrôle à distance", C_WARN, 1); y += 20;
        snprintf(line, sizeof line, "%.28s demande à", S.hello.name); overlay_text(12, y, line, C_TEXT, 1); y += 14;
        overlay_text(12, y, "contrôler la console.", C_TEXT, 1); y += 24;
        overlay_text(12, y, "[A] Autoriser le contrôle", C_ACCENT, 1); y += 14;
        overlay_text(12, y, "[B] Diffusion seule", C_TEXT, 1);
    }
    if (M.msg[0]) overlay_text(12, 222, M.msg, C_WARN, 1);
    overlay_present();
    M.dirty = false;
}

/* ------------------------------------------------------------------ */
/* Session                                                             */
/* ------------------------------------------------------------------ */
static void ctrl_send(u8 type, const u8 *p, u16 n)
{
    u8 buf[SC_MAX_CTRL_PAYLOAD + 3];
    size_t m = sc_write_ctrl(buf, sizeof buf, type, p, n);
    if (S.fd >= 0 && m) send(S.fd, buf, m, 0);
}

static void session_end(const char *why)
{
    if (S.st == S_NONE) return;
    sc_log("session end: %s", why);
    stream_stop(&g_stream);
    if (S.fd >= 0) { ctrl_send(SC_MSG_BYE, NULL, 0); close(S.fd); }
    S.fd = -1; S.st = S_NONE; S.rx_len = 0;
    input_release();
    if (M.st == M_AUTH || M.st == M_AUTH_INPUT) { M.st = M_MAIN; M.dirty = true; }
}

static void session_reject(u8 reason)
{
    ctrl_send(SC_MSG_REJECT, &reason, 1);
    session_end("rejected");
}

static void session_activate(bool input_ok)
{
    u8 model = 0; CFGU_GetSystemModel(&model);
    S.token = (u32)svcGetSystemTick() ^ ((u32)rand() << 8) ^ 0x5C7Au;
    S.input_allowed = input_ok;
    S.input_enabled = true;
    sc_welcome w = { S.token, SC_PROTO_VERSION, SC_PORT_INPUT, (u8)(input_ok && g_cfg.remote_input), model };
    u8 p[SC_WELCOME_SIZE]; sc_welcome_write(p, &w);
    ctrl_send(SC_MSG_WELCOME, p, sizeof p);
    S.q = (sc_quality){ g_cfg.quality, g_cfg.fps_cap, 3, g_cfg.chroma };
    if (!stream_start(&g_stream, S.addr.sin_addr.s_addr, S.hello.video_port, S.token, &S.q)) { session_end("stream start failed"); return; }
    S.st = S_ACTIVE;
    S.t_stats = now_ms();
    N.last_seq = 0; N.remote_active = false;
    sc_log("session active with '%s' input=%d", S.hello.name, input_ok);
    menu_close(); /* "once connected, the game resumes and streaming starts" */
}

static void on_hello(const u8 *p, u16 n)
{
    if (sc_hello_read(p, n, &S.hello) != 0) { session_end("bad hello"); return; }
    if (S.hello.version != SC_PROTO_VERSION) { session_reject(SC_REJECT_VERSION); return; }
    sc_client_id_hex(S.hello.client_id, S.id_hex);
    int trust = sc_config_is_trusted(&g_cfg, S.id_hex);
    if (trust && g_cfg.auto_accept) {
        session_activate(trust == 2 && (S.hello.flags & SC_HELLO_WANTS_INPUT));
        return;
    }
    S.st = S_AUTH; S.t_start = now_ms();
    ctrl_send(SC_MSG_AUTH_PENDING, NULL, 0);
    menu_open(M_AUTH); /* opens (and pauses) if needed */
}

static void on_message(u8 type, const u8 *p, u16 n)
{
    switch (type) {
    case SC_MSG_HELLO: if (S.st == S_HELLO) on_hello(p, n); break;
    case SC_MSG_SET_QUALITY:
        if (S.st == S_ACTIVE && sc_quality_read(p, n, &S.q) == 0) {
            g_stream.quality = S.q.quality; g_stream.fps_cap = S.q.fps_cap;
            g_stream.screens = S.q.screens ? S.q.screens : 3; g_stream.chroma = S.q.chroma;
        }
        break;
    case SC_MSG_SET_INPUT: if (n >= 1) S.input_enabled = p[0] != 0; break;
    case SC_MSG_PING: ctrl_send(SC_MSG_PONG, p, n > SC_PING_SIZE ? SC_PING_SIZE : n); break;
    case SC_MSG_BYE: session_end("bye from PC"); break;
    default: break;
    }
}

static void session_poll_rx(void)
{
    if (S.fd < 0) return;
    int n = recv(S.fd, S.rx + S.rx_len, sizeof(S.rx) - S.rx_len, 0);
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { session_end("tcp closed"); return; }
    if (n > 0) S.rx_len += (size_t)n;
    for (;;) {
        u8 t; const u8 *pl; u16 pn;
        int used = sc_parse_ctrl(S.rx, S.rx_len, &t, &pl, &pn);
        if (used < 0) { session_end("malformed"); return; }
        if (used == 0) break;
        on_message(t, pl, pn);
        if (S.fd < 0) return;
        memmove(S.rx, S.rx + used, S.rx_len - (size_t)used);
        S.rx_len -= (size_t)used;
    }
}

static void session_accept(void)
{
    struct sockaddr_in a; socklen_t al = sizeof a;
    int fd = accept(N.listen_fd, (struct sockaddr *)&a, &al);
    if (fd < 0) return;
    if (S.st != S_NONE) { /* one PC at a time */
        u8 buf[8]; u8 r = SC_REJECT_BUSY;
        size_t m = sc_write_ctrl(buf, sizeof buf, SC_MSG_REJECT, &r, 1);
        send(fd, buf, m, 0); close(fd); return;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    S.fd = fd; S.addr = a; S.st = S_HELLO; S.rx_len = 0; S.t_start = now_ms();
}

static void session_tick(void)
{
    u64 t = now_ms();
    if ((S.st == S_HELLO && t - S.t_start > 5000)) session_end("hello timeout");
    if (S.st == S_AUTH && t - S.t_start > AUTH_TIMEOUT_MS) { session_reject(SC_REJECT_TIMEOUT); snprintf(M.msg, sizeof M.msg, "Demande expirée."); M.dirty = true; }
    if (S.st == S_ACTIVE && t - S.t_stats >= 1000) {
        sc_stats st; stream_take_stats(&g_stream, &st, (u32)(t - S.t_stats));
        u8 p[SC_STATS_SIZE]; sc_stats_write(p, &st);
        ctrl_send(SC_MSG_STATS, p, sizeof p);
        S.t_stats = t;
    }
}

/* ------------------------------------------------------------------ */
/* Keys                                                                */
/* ------------------------------------------------------------------ */
static void keys_tick(void)
{
    u32 k = hid_physical_buttons();
    u32 down = k & ~M.prev_keys;
    M.prev_keys = k;

    if (M.st == M_CLOSED) {
        if ((k & g_cfg.hotkey) == g_cfg.hotkey && (down & g_cfg.hotkey)) {
            sc_log("hotkey pressed (keys=%03lX)", k);
            M.msg[0] = 0; menu_open(M_MAIN);
            sc_log("menu %s, game %s", overlay_is_open() ? "shown" : "NOT shown", M.paused ? "paused" : "running");
        }
        return;
    }
    if (M.st == M_MAIN) {
        if (down & SC_BTN_B) { menu_close(); return; }
        if ((down & SC_BTN_Y) && S.st == S_ACTIVE) { session_end("stopped on console"); snprintf(M.msg, sizeof M.msg, "Diffusion arrêtée."); M.dirty = true; }
        if (down & SC_BTN_X) { g_cfg.remote_input = !g_cfg.remote_input; config_save(); if (!g_cfg.remote_input) input_release(); M.dirty = true; }
        if (down & SC_BTN_SELECT) { session_end("quit"); menu_close(); g_quit = true; }
    } else if (M.st == M_AUTH) {
        if (down & (SC_BTN_A | SC_BTN_Y)) {
            M.remember = (down & SC_BTN_Y) != 0;
            if ((S.hello.flags & SC_HELLO_WANTS_INPUT) && g_cfg.remote_input) { M.st = M_AUTH_INPUT; M.dirty = true; }
            else { if (M.remember) { sc_config_trust(&g_cfg, S.id_hex, 0); config_save(); } session_activate(false); menu_close(); }
        } else if (down & SC_BTN_B) { session_reject(SC_REJECT_DENIED); M.st = M_MAIN; M.dirty = true; }
    } else if (M.st == M_AUTH_INPUT) {
        if (down & (SC_BTN_A | SC_BTN_B)) {
            bool ok = (down & SC_BTN_A) != 0;
            if (M.remember) { sc_config_trust(&g_cfg, S.id_hex, ok); config_save(); }
            session_activate(ok);
            menu_close();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Network setup                                                       */
/* ------------------------------------------------------------------ */
static int udp_bind(u16 port)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&a, sizeof a) < 0) { close(fd); return -1; }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

static bool net_open(void)
{
    N.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (N.listen_fd < 0) return false;
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET; a.sin_port = htons(g_cfg.port); a.sin_addr.s_addr = INADDR_ANY;
    if (bind(N.listen_fd, (struct sockaddr *)&a, sizeof a) < 0 || listen(N.listen_fd, 2) < 0) { close(N.listen_fd); N.listen_fd = -1; return false; }
    fcntl(N.listen_fd, F_SETFL, fcntl(N.listen_fd, F_GETFL, 0) | O_NONBLOCK);
    N.input_fd = udp_bind(SC_PORT_INPUT);
    N.relay_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&N.rosalina, 0, sizeof N.rosalina);
    N.rosalina.sin_family = AF_INET; N.rosalina.sin_port = htons(SC_ROSALINA_INPUT_PORT);
    N.rosalina.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* to verify: loopback on soc:U */
    return N.input_fd >= 0 && N.relay_fd >= 0;
}

static void net_close(void)
{
    session_end("network closed");
    if (N.listen_fd >= 0) close(N.listen_fd);
    if (N.input_fd >= 0) close(N.input_fd);
    if (N.relay_fd >= 0) close(N.relay_fd);
    N.listen_fd = N.input_fd = N.relay_fd = -1;
}

/* ------------------------------------------------------------------ */
int main(void)
{
    sc_log("ScreenCTR " SC_VERSION_STR " starting");
    sc_log("hid check: held=%03lX (should be 000 with no button pressed)", hid_physical_buttons());
    config_load();
    srand((unsigned)svcGetSystemTick());
    if (!stream_alloc()) { sc_log("out of memory"); return 1; }

    u32 *socbuf = memalign(0x1000, SOC_BUF_SIZE);
    if (!socbuf || R_FAILED(socInit(socbuf, SOC_BUF_SIZE))) { sc_log("socInit failed"); return 1; }

    Handle notif = 0;
    srvEnableNotification(&notif);

    bool net_up = false; u64 net_retry = 0;
    while (!g_quit) {
        /* Termination request from PM (shutdown / reboot) */
        if (notif && svcWaitSynchronization(notif, 0) == 0) {
            u32 id = 0;
            if (R_SUCCEEDED(srvReceiveNotification(&id)) && id == 0x100) break;
        }
        u64 t = now_ms();
        if (!net_up && t >= net_retry) {
            if (gethostid() != 0 && net_open()) { net_up = true; sc_log("listening on %u", g_cfg.port); }
            else { net_close(); net_retry = t + 2000; }
        }

        struct pollfd pf[3]; int np = 0;
        if (net_up) {
            pf[np++] = (struct pollfd){ N.listen_fd, POLLIN, 0 };
            pf[np++] = (struct pollfd){ N.input_fd, POLLIN, 0 };
            if (S.fd >= 0) pf[np++] = (struct pollfd){ S.fd, POLLIN, 0 };
            int pr = poll(pf, (nfds_t)np, 4);
            if (pr < 0 && errno != EINTR) { sc_log("poll error %d, network reset", errno); net_close(); net_up = false; net_retry = t + 2000; }
            else {
                if (pf[0].revents & POLLIN) session_accept();
                input_poll(); /* always: also runs the 250 ms release watchdog */
                if (np > 2 && (pf[2].revents & (POLLIN | POLLHUP | POLLERR))) session_poll_rx();
                if (pf[0].revents & (POLLERR | POLLHUP)) { net_close(); net_up = false; net_retry = t + 2000; }
            }
            session_tick();
        } else {
            svcSleepThread(16 * 1000 * 1000);
        }

        keys_tick();
        if (M.st != M_CLOSED && M.dirty) menu_draw();
    }

    net_close();
    menu_close();
    stream_stop(&g_stream);
    socExit();
    sc_log("ScreenCTR stopped");
    return 0;
}
