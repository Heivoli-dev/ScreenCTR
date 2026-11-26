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

/* HID_PAD register: physical 0x10146000, mapped at VA 0x1EC46000 (same VA
 * Rosalina's InputRedirection patches out of the HID module). Bits 0..11 are
 * active low. Requires "IORegisterMapping: 1EC46000" in the RSF. */
#define HID_PAD_REG (*(vu32 *)0x1EC46000)
u32 hid_physical_buttons(void) { return (~HID_PAD_REG) & 0xFFF; }

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

Result proc_foreground(u32 *pid, bool *is_home_menu)
{
    u32 active = 0;
    bool home = R_SUCCEEDED(apt_active_appid(&active)) && active == 0x101; /* APPID_HOMEMENU */
    if (!home) {
        FS_ProgramInfo info; u32 apid = 0, flags = 0;
        if (R_SUCCEEDED(PMDBG_GetCurrentAppInfo(&info, &apid, &flags)) && apid) {
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
