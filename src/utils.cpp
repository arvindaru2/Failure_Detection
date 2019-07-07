#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include "utils.hpp"

// From our MP1
void grep_log(const char *fmt, ...)
{
  FILE *log_file = fopen(MPLOG_LOG_FILE, "a");
  if (log_file == NULL) {
    fprintf(stderr, "MPLOG ERROR: Unable to open log file %s for writing\n", MPLOG_LOG_FILE);
    return;
  }

  va_list argp;
  va_start(argp, fmt);
  // vfprintf(log_file, fmt, argp);
  vfprintf(stdout, fmt, argp);
  va_end(argp);

  if (fclose(log_file) != 0) {
    fprintf(stderr, "MPLOG ERROR: Unable to close log file: %s\n", strerror(errno));
    return;
  }
}

// From our MP1
int get_server_number(void)
{
  char name[1024];
  if (gethostname(&name[0], sizeof(name)) != 0) {
    // Failure
    MPLOG("Unable to get hostname: %s", strerror(errno));
    return -1;
  }
  name[sizeof(name) - 1] = '\0';

  // Parse our hostname to get our server number
  unsigned servNum;
  if (sscanf(name, "fa17-cs425-g18-%02u.cs.illinois.edu", &servNum) == 1) {
    // Success
    MPLOG("Got hostname %s", name);
    return static_cast<int>(servNum);
  }

  // Failure
  MPLOG("Error parsing the server number from our hostname");
  return -1;
}

