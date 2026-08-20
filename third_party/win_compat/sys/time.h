#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_WINSOCK2API_) && !defined(_WINSOCKAPI_)
#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED
struct timeval {
    long tv_sec;
    long tv_usec;
};
#endif
#endif

#ifndef _TIMEZONE_DEFINED
#define _TIMEZONE_DEFINED
struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};
#endif

int gettimeofday(struct timeval *tv, struct timezone *tz);

#ifdef __cplusplus
}
#endif
