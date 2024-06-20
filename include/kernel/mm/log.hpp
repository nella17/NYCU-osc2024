#pragma once

#define MM_LOG_LEVEL 1
#include "io.hpp"

#if MM_LOG_LEVEL >= 3
#define MM_DEBUG_PRINT(fmt, ...) klog(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define MM_DEBUG_PRINT(fmt, ...) 0
#endif

#define MM_DEBUG(fmt, ...) \
  MM_DEBUG_PRINT("[DEBUG] [" MM_TYPE "] " fmt __VA_OPT__(, ) __VA_ARGS__)

#if MM_LOG_LEVEL >= 2
#define MM_VERBOSE_PRINT(fmt, ...) klog(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define MM_VERBOSE_PRINT(fmt, ...) 0
#endif

#define MM_VERBOSE(fmt, ...) \
  MM_VERBOSE_PRINT("[VERBOSE] [" MM_TYPE "] " fmt __VA_OPT__(, ) __VA_ARGS__)

#if MM_LOG_LEVEL >= 1
#define MM_INFO_PRINT(fmt, ...) klog(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define MM_INFO_PRINT(fmt, ...) 0
#endif

#define MM_INFO(fmt, ...) \
  MM_INFO_PRINT("[*] [" MM_TYPE "] " fmt __VA_OPT__(, ) __VA_ARGS__)
