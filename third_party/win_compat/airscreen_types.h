#pragma once

#include <BaseTsd.h>

#ifndef _SSIZE_T_DEFINED
typedef SSIZE_T ssize_t;
#define _SSIZE_T_DEFINED
#endif

#ifndef strdup
#define strdup _strdup
#endif

#ifndef SSIZE_MAX
#ifdef _WIN64
#define SSIZE_MAX 9223372036854775807LL
#else
#define SSIZE_MAX 2147483647
#endif
#endif
