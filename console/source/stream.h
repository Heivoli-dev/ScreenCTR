/* ScreenCTR console — Copyright (C) 2026 Heivoli Network — GPL-3.0-or-later */
#pragma once
#include <3ds.h>
#include <stdbool.h>
#include <netinet/in.h>
#include "screenctr_protocol.h"

typedef struct {
    /* settings (written by the main thread, read by the stream thread) */
    volatile u8 quality, fps_cap, screens, chroma;
    volatile bool paused;
    /* statistics, reset by stream_take_stats() */
    volatile u32 frames[2], enc_us[2], bytes, skipped;
    /* private */
    volatile bool run;
    int sock;
    struct sockaddr_in dst;
    u32 token;
    Thread thread;
    int core;
} streamer;

bool stream_alloc(void);   /* one-time buffer allocation, at boot */
bool stream_start(streamer *s, u32 pc_ip_be, u16 pc_port, u32 token, const sc_quality *q);
void stream_stop(streamer *s);
void stream_take_stats(streamer *s, sc_stats *out, u32 elapsed_ms);
