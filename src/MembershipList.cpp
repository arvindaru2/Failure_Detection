#include <cstring>
#include "MembershipList.hpp"
#include "utils.hpp"

int g18::MembershipList::nodeDidJoin(const node_id_t &node)
{
  // Check if we already have this node
  const auto existingIt = lookUp(node.ip);
  if (existingIt != members.end()) {
    const membership_entry_t &existing = *existingIt;
    // Make sure our existing copy is the same as the new one
    if (existing.id.timestamp > node.timestamp) {
      MPLOG("ERROR: Attempting to add node %u with timestamp %u when we have a newer version with timestamp %u",
            node.ip, node.timestamp, existing.id.timestamp);
      return -1;
    } else if (existing.id.timestamp < node.timestamp) {
      if (existing.state == NODE_STATE_ONLINE) {
        MPLOG("ERROR: Attempting to add node %u with timestamp %u but an older version is still online with timestamp %u",
              node.ip, node.timestamp, existing.id.timestamp);
        return -1;
      }
      // Continue on to add the node
    } else { // Timestamps match
      if (existing.state != NODE_STATE_ONLINE) {
        MPLOG("ERROR: Attempting to add node %u which already exists in state %s",
              node.ip, strNodeState(existing.state));
        return -1;
      }
      // They match, nothing left to do here
      return 0;
    }
  }
  // This node does not yet exist, so we add it
  MPLOG("Debug: Node %d joining", node.ip);
  members.push_back((membership_entry_t){
    .id = node,
    .state = NODE_STATE_ONLINE
  });
  return 1;
}

int g18::MembershipList::nodeDidLeave(const node_id_t &node)
{
  MPLOG("Debug: Node %d leaving", node.ip);
  return killNodeImpl(node, NODE_STATE_DEPARTED);
}

int g18::MembershipList::nodeDidDie(const node_id_t &node)
{
  MPLOG("Debug: Node %d dying", node.ip);
  return killNodeImpl(node, NODE_STATE_DIED);
}

bool g18::MembershipList::hasSuccessor(const node_id_t &node)
{
  return successorOfImpl(node) != members.end();
}

bool g18::MembershipList::hasPredecessor(const node_id_t &node)
{
  return predecessorOfImpl(node) != members.end();
}

node_id_t g18::MembershipList::successorOf(const node_id_t &node)
{
  std::list<membership_entry_t>::iterator it = successorOfImpl(node);
  if (it == members.end()) {
    // Not found
    node_id_t nid;
    memset(&nid, 0, sizeof(nid));
    return nid;
  }
  return it->id;
}

node_id_t g18::MembershipList::predecessorOf(const node_id_t &node)
{
  std::list<membership_entry_t>::iterator it = predecessorOfImpl(node);
  if (it == members.end()) {
    // Not found
    node_id_t nid;
    memset(&nid, 0, sizeof(nid));
    return nid;
  }
  return it->id;
}

std::list<membership_entry_t>::iterator g18::MembershipList::successorOfImpl(const node_id_t &node)
{
  auto queryNodeIt = lookUp(node), it = queryNodeIt;
  // Walk the list
  if (it != members.end()) {
    // Skip over dead nodes
    while (++it != members.end()) {
      if (it->state == NODE_STATE_ONLINE) {
        return it;
      }
    }
    // Wrap around to the beginning if necessary
    it = members.begin();
    if (it != queryNodeIt) {
      do {
        if (it->state == NODE_STATE_ONLINE) {
          return it;
        }
      } while (++it != queryNodeIt);
    }
  }
  // Not found
  return members.end();
}

std::list<membership_entry_t>::iterator g18::MembershipList::predecessorOfImpl(const node_id_t &node)
{
  auto queryNodeIt = lookUp(node), it = queryNodeIt;
  if (it != members.end()) {
    // Skip over dead nodes
    if (it-- != members.begin()) {
      do {
        if (it->state == NODE_STATE_ONLINE) {
          return it;
        }
      } while (it-- != members.begin());
    }
    // Wrap around to the end if necessary
    it = members.end();
    while (--it != queryNodeIt) {
      if (it->state == NODE_STATE_ONLINE) {
        return it;
      }
    }
  }
  // Not found
  return members.end();
}

int g18::MembershipList::killNodeImpl(const node_id_t &node,
                                      const node_state_e desiredState)
{
  // Check if we already have this node
  const auto existingIt = lookUp(node.ip);
  if (existingIt != members.end()) {
    const membership_entry_t &existing = *existingIt;
    // Make sure our existing copy is the same as the new one
    if (existing.id.timestamp != node.timestamp) {
      MPLOG("ERROR: Node %u with timestamp %u reported gone, but we only have that node with timestamp %u",
            node.ip, node.timestamp, existing.id.timestamp);
      return -1;
    }
    if (existing.state != NODE_STATE_ONLINE) {
      MPLOG("ERROR: Attempting to remove node %u which already exists in state %s",
            node.ip, strNodeState(existing.state));
      return -1;
    }
    // They match. Mark it as gone.
    membership_entry_t newEntry = existing;
    newEntry.state = desiredState;
    *existingIt = newEntry;
    return 0;
  } else {
    // This node does not yet exist, so it has no business dying
    MPLOG("ERROR: Node %u doesn't exist, but it has been reported dead",
          node.ip);
    return -1;
  }
}

std::list<membership_entry_t>::iterator g18::MembershipList::lookUp(const persistent_node_id_t ip)
{
  auto it = members.begin();
  for (; it != members.end(); ++it) {
    if (it->id.ip == ip) {
      return it;
    }
  }
  return it;
}

std::list<membership_entry_t>::iterator g18::MembershipList::lookUp(const node_id_t &node)
{
  auto it = members.begin();
  for (; it != members.end(); ++it) {
    if (isEqual(it->id, node)) {
      return it;
    }
  }
  return it;
}

const char * g18::MembershipList::strNodeState(const node_state_e state) const
{
  switch (state) {
  case NODE_STATE_ONLINE:
    return "ONLINE";
  case NODE_STATE_DEPARTED:
    return "DEPARTED";
  case NODE_STATE_DIED:
    return "DIED";
  }
  // Unknown state
  return "(INVALID STATE)";
}
