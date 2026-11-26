/* ScreenCTR Launcher — starts the ScreenCTR sysmodule in the background.
 * Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include <3ds.h>
#include <stdio.h>
#include <string.h>

#define SCREENCTR_TID 0x0004013000A5C702ULL

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

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("\x1b[32mScreenCTR\x1b[0m  Two screens. One connection.\n\n");

    if (is_running()) {
        printf("ScreenCTR est deja en cours d'execution.\n");
    } else {
        nsInit();
        u32 pid = 0;
        Result r = NS_LaunchTitle(SCREENCTR_TID, 0, &pid);
        nsExit();
        if (R_SUCCEEDED(r)) printf("ScreenCTR demarre (PID %lu).\n", pid);
        else printf("\x1b[31mEchec du demarrage : %08lX\x1b[0m\nScreenCTR.cia est-il installe ?\n", r);
    }
    /* Diagnostics: is the sysmodule alive, and what did it log? */
    svcSleepThread(1500000000LL);
    printf("\nEtat : %s\n", is_running() ? "\x1b[32mScreenCTR tourne en arriere-plan\x1b[0m"
                                          : "\x1b[31mScreenCTR ne tourne PAS (arret au demarrage ?)\x1b[0m");
    FILE *f = fopen("sdmc:/3ds/ScreenCTR/log.txt", "r");
    if (!f) printf("Pas de log.txt (sd:/3ds/ScreenCTR/)\n");
    else {
        static char lines[10][100]; int n = 0;
        char buf[100];
        while (fgets(buf, sizeof buf, f)) { strncpy(lines[n % 10], buf, 99); n++; }
        fclose(f);
        printf("--- fin de log.txt ---\n");
        for (int i = n > 10 ? n - 10 : 0; i < n; i++) printf("%.49s", lines[i % 10]), (strchr(lines[i % 10], '\n') ? 0 : printf("\n"));
    }
    printf("\nOuvrez un jeu puis L+R+START.\nSTART : quitter\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
