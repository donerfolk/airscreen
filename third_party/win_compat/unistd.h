#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include "sys/time.h"

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 0
#endif

int usleep(unsigned int usec);
unsigned int sleep(unsigned int seconds);
int clock_gettime(int clk_id, struct timespec *tp);

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#ifdef __cplusplus
}
#endif
