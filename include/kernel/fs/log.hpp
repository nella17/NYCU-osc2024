#pragma once

#define FS_LOG_LEVEL 1
#include "io.hpp"

#if FS_LOG_LEVEL >= 1
#define FS_INFO_PRINT(fmt, ...) klog(fmt __VA_OPT__(, ) __VA_ARGS__)
#else
#define FS_INFO_PRINT(fmt, ...) 0
#endif

#define FS_INFO(fmt, ...) \
  FS_INFO_PRINT("[" FS_TYPE "] [*] " fmt __VA_OPT__(, ) __VA_ARGS__)

#define FS_WARN_PRINT(fmt, ...) klog(fmt __VA_OPT__(, ) __VA_ARGS__)

#define FS_WARN(fmt, ...) \
  FS_WARN_PRINT("[" FS_TYPE "] [!] " fmt __VA_OPT__(, ) __VA_ARGS__)

#define FS_WARN_IF(cond, fmt, ...)             \
  do {                                         \
    if (cond)                                  \
      FS_WARN(fmt __VA_OPT__(, ) __VA_ARGS__); \
    else                                       \
      FS_INFO(fmt __VA_OPT__(, ) __VA_ARGS__); \
  } while (0)
