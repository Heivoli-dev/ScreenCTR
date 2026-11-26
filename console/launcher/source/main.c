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
    printf("\nOuvrez un jeu puis appuyez sur L+R+START\npour afficher le menu ScreenCTR.\n\nSTART : retour au menu HOME\n");
    while (aptMainLoop()) {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
