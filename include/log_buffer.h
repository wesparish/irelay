#pragma once
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <Arduino.h>

// LOG_ROLE must be defined before including this header (e.g. "receiver" or "emitter").
// Entry layout: "[receiver] [HH:MM:SS] <message>"
//   "[receiver] " = 12 chars (longest role label)
//   "[HH:MM:SS] " = 11 chars
//   total prefix  = 23 chars
#ifndef LOG_ROLE
#define LOG_ROLE "node"
#endif

#define LOG_BUF_ENTRIES   30u
#define LOG_BUF_ENTRY_LEN 96u
#define LOG_PREFIX_LEN    23u  // "[receiver] [HH:MM:SS] "

// Circular ring buffer — s_logHead points to the next write slot.
// When full the oldest entry is overwritten.
static char     s_logBuf[LOG_BUF_ENTRIES][LOG_BUF_ENTRY_LEN];
static uint8_t  s_logHead  = 0;
static uint32_t s_logTotal = 0;  // monotonic count of entries ever pushed

// Prepend "[<role>] [HH:MM:SS] ", then store msg (truncated to fit).
inline void logPush(const char* msg) {
    uint32_t s = millis() / 1000;
    uint32_t h = s / 3600; s %= 3600;
    uint32_t m = s / 60;   s %= 60;
    snprintf(s_logBuf[s_logHead], LOG_BUF_ENTRY_LEN,
        "[" LOG_ROLE "] [%02u:%02u:%02u] %s", h, m, s, msg);
    s_logHead = (s_logHead + 1) % LOG_BUF_ENTRIES;
    s_logTotal++;
}

// Formatted push — message portion is capped at (LOG_BUF_ENTRY_LEN - LOG_PREFIX_LEN - 1) chars.
inline void logPushf(const char* fmt, ...) {
    char buf[LOG_BUF_ENTRY_LEN - LOG_PREFIX_LEN];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    logPush(buf);
}
