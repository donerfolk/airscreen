#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdint.h>
#include <time.h>
#include "unistd.h"
#include "sys/time.h"

int usleep(unsigned int usec) {
    Sleep((usec + 999) / 1000);
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    Sleep(seconds * 1000);
    return 0;
}

int clock_gettime(int clk_id, struct timespec *tp) {
    FILETIME ft;
    ULARGE_INTEGER uli;
    uint64_t t;
    (void) clk_id;
    if (!tp) {
        return -1;
    }
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    t = uli.QuadPart - 116444736000000000ULL;
    tp->tv_sec = (time_t) (t / 10000000ULL);
    tp->tv_nsec = (long) ((t % 10000000ULL) * 100);
    return 0;
}

int gettimeofday(struct timeval *tv, struct timezone *tz) {
    FILETIME ft;
    ULARGE_INTEGER uli;
    uint64_t t;
    (void) tz;
    if (!tv) {
        return -1;
    }
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    t = uli.QuadPart - 116444736000000000ULL;
    tv->tv_sec = (long) (t / 10000000ULL);
    tv->tv_usec = (long) ((t % 10000000ULL) / 10);
    return 0;
}
