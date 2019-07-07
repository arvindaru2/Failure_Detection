
#pragma once

#include <cstdarg>

#define _MPLOG(fmt, ...) do { \
  grep_log("%s:%s:%d: " fmt "\n", \
      __FILE__, __FUNCTION__, __LINE__ , ## __VA_ARGS__); \
} while (0)
#define MPLOG(fmt, ...) _MPLOG(fmt , ## __VA_ARGS__)

#define MPLOG_LOG_FILE "mplog.log"

void grep_log(const char *fmt, ...);

/// Parse our server number from our hostname.
int get_server_number(void);

