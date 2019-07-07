#pragma once
#include <list>
#include <stdint.h>

/// Lamport time
typedef uint16_t lamp_time_t;

typedef uint32_t persistent_node_id_t; // The node's IP address

typedef struct __attribute__((packed)) {
  persistent_node_id_t ip;
  lamp_time_t timestamp;
} node_id_t;

typedef enum {
  NODE_STATE_ONLINE,
  NODE_STATE_DEPARTED,
  NODE_STATE_DIED
} node_state_e;

/////////////////////////////////////////
// Network Types
/////////////////////////////////////////

#define FORWARD_PROP_PORT_STR "31337"
#define BACK_PROP_PORT_STR "31338"

// A listing of all changes that we have not yet seem come full circle around the ring. For local use only; not in network format.
typedef struct {
  std::list<node_id_t> joined, left, failed;
  lamp_time_t timestamp;
} changelist_t;

namespace g18 {
  bool isEqual(const node_id_t &lhs, const node_id_t &rhs);
  bool changelistIsEmpty(const changelist_t &lst);
}

#define MAX(a,b) ((a) > (b) ? (a) : (b))
