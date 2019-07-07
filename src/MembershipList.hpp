#pragma once
#include <list>
#include "net_types.hpp"

typedef struct __attribute__((packed)) {
  node_id_t id;
  node_state_e state;
} membership_entry_t;

namespace g18 {
  class MembershipList {
    public:
      /// Call this with each node that may possibly be joining. This method is
      /// idempotent. Returns -1 on error, 0 on no change, 1 if the node was added.
      int nodeDidJoin(const node_id_t &node);

      /// Call this with each node that may possibly have left. This method is idempotent.
      int nodeDidLeave(const node_id_t &node);

      /// Call this with each node that may possibly have died. This method is idempotent.
      int nodeDidDie(const node_id_t &node);

      /// Check if the node after this one exists and is alive.
      bool hasSuccessor(const node_id_t &node);

      /// Check if the node before this one exists and is alive.
      bool hasPredecessor(const node_id_t &node);

      /// Get the node after this one.
      node_id_t successorOf(const node_id_t &node);

      /// Get the node before this one.
      node_id_t predecessorOf(const node_id_t &node);

    private:
      std::list<membership_entry_t> members;

      int killNodeImpl(const node_id_t &node,
                       const node_state_e desiredState);

      std::list<membership_entry_t>::iterator lookUp(const node_id_t &node);
      std::list<membership_entry_t>::iterator lookUp(const persistent_node_id_t ip);
      std::list<membership_entry_t>::iterator successorOfImpl(const node_id_t &node);
      std::list<membership_entry_t>::iterator predecessorOfImpl(const node_id_t &node);

      /// Return a string representation of a node state.
      const char * strNodeState(const node_state_e state) const;
  };
}
