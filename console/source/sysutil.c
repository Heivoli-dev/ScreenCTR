/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#include "sysutil.h"
#include "csvc.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static LightLock g_logLock;
static bool g_logInit;

void sc_log(const char *fmt, ...)
{
    if (!g_logInit) { LightLock_Init(&g_logLock); g_logInit = true; }
    LightLock_Lock(&g_logLock);
    FILE *f = fopen(SC_LOG_PATH, "a");
    if (f) {
        fprintf(f, "[%llu] ", (unsigned long long)now_ms());
        va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
        fputc('\n', f);
        fclose(f);
    }
    LightLock_Unlock(&g_logLock);
}

/* Same method as Rosalina's menu thread: hidInit() once (in __appInit), then
 * hidScanInput() + hidKeysHeld(). Bits 0..11 match SC_BTN_* (KEY_A..KEY_Y).
 * Note: with Rosalina InputRedirection active, remote buttons are included;
 * ScreenCTR ignores remote input while its menu is open. */
u32 hid_physical_buttons(void)
{
    hidScanInput();
    return hidKeysHeld() & 0xFFF;
}

Result proc_find_by_name(const char *name, u32 *pid)
{
    u32 pids[0x40]; s32 n = 0;
    Result r = svcGetProcessList(&n, pids, 0x40);
    if (R_FAILED(r)) return r;
    for (s32 i = 0; i < n; i++) {
        Handle h;
        if (R_FAILED(svcOpenProcess(&h, pids[i]))) continue;
        char pname[9] = {0};
        s64 v = 0;
        /* 0x10000: Luma3DS extension, returns the 8-byte process name */
        Result rr = svcGetProcessInfo(&v, h, 0x10000);
        svcCloseHandle(h);
        if (R_FAILED(rr)) continue;
        memcpy(pname, &v, 8);
        if (strncmp(pname, name, 8) == 0) { *pid = pids[i]; return 0; }
    }
    return -1;
}

/* APT:U GetAppletManInfo(APTPOS_NONE): word[5] = active (foreground) applet id.
 * Raw IPC so we do not need aptInit(), which a sysmodule must not call. */
static Result apt_active_appid(u32 *active)
{
    Handle h;
    Result r = srvGetServiceHandle(&h, "APT:U");
    if (R_FAILED(r)) return r;
    u32 *cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x5, 1, 0);
    cmd[1] = (u32)-1; /* APTPOS_NONE */
    r = svcSendSyncRequest(h);
    if (R_SUCCEEDED(r)) r = (Result)cmd[1];
    if (R_SUCCEEDED(r)) *active = cmd[5];
    svcCloseHandle(h);
    return r;
}

/* pm:dbg command 0x100 "GetCurrentAppInfo": a Luma3DS extension of its PM
 * reimplementation (not in libctru). Response: [1] result, [2..5] FS_ProgramInfo,
 * [6] pid, [7] launch flags — as in Rosalina's pmdbgext.c. */
static Result pmdbg_current_app_pid(u32 *pid)
{
    u32 *cmd = getThreadCommandBuffer();
    cmd[0] = IPC_MakeHeader(0x100, 0, 0);
    Result r = svcSendSyncRequest(*pmDbgGetSessionHandle());
    if (R_FAILED(r)) return r;
    if (R_SUCCEEDED((Result)cmd[1])) *pid = cmd[6];
    return (Result)cmd[1];
}

Result proc_foreground(u32 *pid, bool *is_home_menu)
{
    u32 active = 0;
    bool home = R_SUCCEEDED(apt_active_appid(&active)) && active == 0x101; /* APPID_HOMEMENU */
    if (!home) {
        u32 apid = 0;
        if (R_SUCCEEDED(pmdbg_current_app_pid(&apid)) && apid) {
            *pid = apid; if (is_home_menu) *is_home_menu = false;
            return 0;
        }
    }
    if (is_home_menu) *is_home_menu = true;
    return proc_find_by_name("menu", pid);
}

Result proc_set_paused(u32 pid, bool paused)
{
    Handle h;
    Result r = svcOpenProcess(&h, pid);
    if (R_FAILED(r)) return r;
    /* Luma3DS svcControlProcess: same mechanism Rosalina uses to freeze the
     * game while its menu is open. */
    r = svcControlProcess(h, PROCESSOP_SCHEDULE_THREADS, paused ? 1 : 0, 0);
    svcCloseHandle(h);
    return r;
}
