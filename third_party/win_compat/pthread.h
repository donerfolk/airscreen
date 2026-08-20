#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#include <winsock2.h>
#endif
#include <windows.h>
#include <process.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef ETIMEDOUT
#define ETIMEDOUT 138
#endif

typedef uintptr_t pthread_t;
typedef int pthread_attr_t;
typedef CRITICAL_SECTION pthread_mutex_t;
typedef CONDITION_VARIABLE pthread_cond_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;

typedef struct pthread_start_s {
    void *(*fn)(void *);
    void *arg;
} pthread_start_t;

static unsigned __stdcall airscreen_pthread_thunk(void *param) {
    pthread_start_t *st = (pthread_start_t *) param;
    void *(*fn)(void *) = st->fn;
    void *arg = st->arg;
    free(st);
    fn(arg);
    return 0;
}

static inline int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    pthread_start_t *st;
    uintptr_t handle;
    (void) attr;
    if (!thread || !start_routine) {
        return EINVAL;
    }
    st = (pthread_start_t *) malloc(sizeof(*st));
    if (!st) {
        return ENOMEM;
    }
    st->fn = start_routine;
    st->arg = arg;
    handle = _beginthreadex(NULL, 0, airscreen_pthread_thunk, st, 0, NULL);
    if (!handle) {
        free(st);
        return EAGAIN;
    }
    *thread = handle;
    return 0;
}

static inline int pthread_join(pthread_t thread, void **retval) {
    (void) retval;
    if (!thread) {
        return EINVAL;
    }
    WaitForSingleObject((HANDLE) thread, INFINITE);
    CloseHandle((HANDLE) thread);
    return 0;
}

static inline int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    (void) attr;
    InitializeCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    DeleteCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *mutex) {
    EnterCriticalSection(mutex);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    LeaveCriticalSection(mutex);
    return 0;
}

static inline int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    (void) attr;
    InitializeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_destroy(pthread_cond_t *cond) {
    (void) cond;
    return 0;
}

static inline int pthread_cond_signal(pthread_cond_t *cond) {
    WakeConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_broadcast(pthread_cond_t *cond) {
    WakeAllConditionVariable(cond);
    return 0;
}

static inline int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    SleepConditionVariableCS(cond, mutex, INFINITE);
    return 0;
}

static inline int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    struct timespec now;
    int64_t ms;
    FILETIME ft;
    ULARGE_INTEGER uli;
    uint64_t t;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    t = uli.QuadPart - 116444736000000000ULL;
    now.tv_sec = (time_t) (t / 10000000ULL);
    now.tv_nsec = (long) ((t % 10000000ULL) * 100);
    ms = ((int64_t) abstime->tv_sec - (int64_t) now.tv_sec) * 1000;
    ms += ((int64_t) abstime->tv_nsec - (int64_t) now.tv_nsec) / 1000000;
    if (ms < 0) {
        ms = 0;
    }
    if (SleepConditionVariableCS(cond, mutex, (DWORD) ms)) {
        return 0;
    }
    if (GetLastError() == ERROR_TIMEOUT) {
        return ETIMEDOUT;
    }
    return EINVAL;
}

#ifdef __cplusplus
}
#endif
