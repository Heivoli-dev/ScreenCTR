/* ScreenCTR Launcher — starts the ScreenCTR sysmodule and shows its state.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define SCREENCTR_TID 0x0004013000A5C702ULL
#define LOG_PATH "sdmc:/3ds/ScreenCTR/log.txt"

#define GREEN "\x1b[32m"
#define RED "\x1b[31m"
#define YELLOW "\x1b[33m"
#define RESET "\x1b[0m"

static bool is_running(void)
{
    u32 pids[0x40]; s32 n = 0;
    if (R_FAILED(svcGetProcessList(&n, pids, 0x40))) return false;
    for (s32 i = 0; i < n; i++) {
        Handle h; s64 name = 0;
        if (R_FAILED(svcOpenProcess(&h, pids[i]))) continue;
        Result r = svcGetProcessInfo(&name, h, 0x10000); /* Luma3DS: process name */
        svcCloseHandle(h);
        if (R_SUCCEEDED(r) && memcmp(&name, "ScrnCTR", 7) == 0) return true;
    }
    return false;
}

/* What the sysmodule reported since it started (its log is reset at boot). */
typedef struct {
    bool started, hid_ok, hid_checked, listening, hotkey, menu_shown, menu_failed, socfail, oom;
    char addr[40];
} sysstate;

static void read_state(sysstate *s)
{
    memset(s, 0, sizeof(*s));
    FILE *f = fopen(LOG_PATH, "r");
    if (!f) return;
    char line[160];
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, " starting")) s->started = true;
        char *p;
        if ((p = strstr(line, "hid check: held="))) { s->hid_checked = true; s->hid_ok = strncmp(p + 16, "000", 3) == 0; }
        if ((p = strstr(line, "listening on "))) {
            s->listening = true;
            snprintf(s->addr, sizeof s->addr, "%s", p + 13);
            s->addr[strcspn(s->addr, "\r\n")] = 0;
        }
        if (strstr(line, "hotkey pressed")) s->hotkey = true;
        if (strstr(line, "menu shown")) { s->menu_shown = true; s->menu_failed = false; }
        if (strstr(line, "menu NOT shown")) { s->menu_failed = true; }
        if (strstr(line, "socInit failed")) s->socfail = true;
        if (strstr(line, "out of memory")) s->oom = true;
    }
    fclose(f);
}

static void draw(bool running, const sysstate *s, Result launch_rc)
{
    consoleClear();
    printf(GREEN "ScreenCTR" RESET "  Two screens. One connection.\n");
    printf("Heivoli Network\n\n");

    if (R_FAILED(launch_rc)) {
        printf(RED "Demarrage impossible (%08lX)\n" RESET, launch_rc);
        printf("Reinstallez ScreenCTR.cia avec FBI.\n");
        return;
    }
    printf("ScreenCTR   : %s\n", running ? GREEN "actif" RESET : RED "arrete" RESET);
    if (!running) {
        if (s->oom) printf(RED "  -> memoire insuffisante\n" RESET);
        else if (s->socfail) printf(RED "  -> reseau indisponible\n" RESET);
        else if (!s->started) printf(RED "  -> arret avant meme de demarrer\n" RESET);
        printf("\nRedemarrez la console puis relancez\nce lanceur. Si cela persiste, envoyez\nune photo de cet ecran.\n");
        return;
    }
    printf("Wi-Fi       : %s\n", s->listening ? GREEN "connecte" RESET : YELLOW "en attente..." RESET);
    if (s->listening) printf("Adresse PC  : " GREEN "%s" RESET "\n", s->addr);
    printf("Boutons     : %s\n", !s->hid_checked ? YELLOW "?" RESET : s->hid_ok ? GREEN "OK" RESET : RED "anomalie" RESET);
    if (s->hotkey)
        printf("L+R+START   : %s\n", s->menu_shown ? GREEN "menu affiche" RESET
                                     : s->menu_failed ? RED "detecte, menu NON affiche" RESET : YELLOW "detecte" RESET);
    printf("\nDans un jeu : L+R+START ouvre le menu.\n");
    printf("Sur le PC : saisissez l'adresse ci-dessus.\n");
}

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);

    Result rc = 0;
    if (!is_running()) {
        nsInit();
        u32 pid = 0;
        rc = NS_LaunchTitle(SCREENCTR_TID, 0, &pid);
        nsExit();
    }

    printf("Demarrage de ScreenCTR...\n");
    u64 next = 0;
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & (KEY_START | KEY_B)) break;
        u64 now = osGetTime();
        if (now >= next) { /* refresh twice a second: Wi-Fi may connect later */
            sysstate s; read_state(&s);
            draw(is_running(), &s, rc);
            printf("\nSTART : retour au menu HOME\n");
            next = now + 500;
        }
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
