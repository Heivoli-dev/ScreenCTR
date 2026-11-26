/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#pragma once
#include <3ds.h>
#include <stdbool.h>

#define SC_LOG_PATH   "sdmc:/3ds/ScreenCTR/log.txt"
#define SC_CFG_DIR    "sdmc:/3ds/ScreenCTR"
#define SC_CFG_PATH   "sdmc:/3ds/ScreenCTR/config.ini"

void sc_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static inline u64 now_ms(void) { return osGetTime(); }

/* Physical buttons, read straight from the HID I/O register (mapped through
 * the exheader). Remote input redirected by Rosalina never shows up here, so
 * the PC cannot open the ScreenCTR menu or answer its prompts. Active high. */
u32  hid_physical_buttons(void);

/* Process helpers (Luma3DS extended SVCs) */
Result proc_find_by_name(const char *name, u32 *pid);
/* PID of the process that currently owns the screens: the running
 * application, or the HOME Menu when it is in front. */
Result proc_foreground(u32 *pid, bool *is_home_menu);
Result proc_set_paused(u32 pid, bool paused);
