#include <cerrno>
#include <cstring>
#include <pthread.h>
#include "Daemon.hpp"
#include "utils.hpp"

using g18::Daemon;

static void startREPL(Daemon &daemon) __attribute__((noreturn));

/// Take orders from the command line.
static void startREPL(Daemon &daemon)
{
  char user_input[2];
  do {
    // Display a prompt
    printf("%u> ", daemon.getPersistentID());
    // Get the user's action
    char *ret = fgets(user_input, sizeof(user_input), stdin);
    if (ret == NULL) {
      // Got EOF; exit
      MPLOG("REPL got EOF; terminating");
      exit(1);
    }
    // Perform the user's action
    switch (user_input[0]) {
    // Quit
    case 'k':
    case 'q':
      daemon.killSelf();
      break;

    // Leave group
    case 'l':
      daemon.leaveGroup();
      break;

    case 'h':
    default:
      // Print help message
      printf("h\tDisplay this help message\n");
      printf("l\tLeave the group peacefully after giving notice\n");
      printf("k\tKill ourself without notice\n");
      printf("q\tSynonym for k\n");
      break;
    }
  } while (true);
}

int main(int argc, const char *argv[])
{
  // Get our ID number
  persistent_node_id_t ourID = 0;
  if (argc > 1) {
    const char *id_num_str = argv[1], *c = id_num_str;
    while (isdigit(*c)) {
      c++;
    }
    if (*c == '\0') {
      ourID = atol(id_num_str);
    }
  }
  // Fall back onto our hostname
  if (ourID == 0) {
    ourID = static_cast<persistent_node_id_t>(get_server_number());
  }

  Daemon daemon = Daemon(ourID);
  // Start the REPL
  startREPL(daemon);
}

