#pragma once
#include <pthread.h>
#include <string>
#include <vector>
#include "MembershipList.hpp"
#include "net_types.hpp"

namespace g18 {
  class Daemon {
    public:
      /// The persistent identifier of the recruiter.
      static const persistent_node_id_t recruiterID = 1;

      /// Create a new Daemon with the given persistent identifier.
      Daemon(const persistent_node_id_t persistentID);

      /// Get the persistent ID of the node to which we should send our next backpropagation message.
      persistent_node_id_t getBackpropagationTarget();

      /// Update our internal state based on the contents of a received heartbeat.
      void handleReceivedHeartbeat(const std::string &hb);

      /// Do something if a neighbor skips a heartbeat.
      void handleMissedHeartbeat();

      /// Update our internal state based on the contents of a received message.
      void handleReceivedBackpropagationMessage(const std::string &bp);

      /// Take appropriate action to allow a new node to join the group.
      /// Only valid when this Daemon is the recruiter for the group.
      void handleNodeJoinRequest(const std::string &bp);

      /// Generate a message to be sent as a heartbeat.
      std::string generateMessageForHeartbeat() const;

      /// Generate a message to be sent containing all of our internal updates.
      std::string generateMessageForBackpropagation();

      /// Add the node to the delta with the specified action, but only if not
      /// already present.
      void addToDelta(const node_id_t &node, const node_state_e state);

      /// Attempt to join the group.
      void joinGroup();

      /// Notify the group that we're leaving, then leave the group.
      void leaveGroup();

      /// Call this when we want to kill ourself without notifying the group.
      void killSelf() const __attribute__((noreturn));

      /// Asynchronously start sending periodic heartbeats to whomever's our
      /// next neighbor over in the group.
      void beginHeartbeating() const;

      /// Asynchronously start listening for periodic heartbeats.
      void beginExpectingHeartbeats() const;

      /// Asynchronously start listening for spurious backpropagated messages.
      void beginExpectingBackpropagatedMessages() const;

      /// Send a single heartbeat. Returns 0 on success, -1 on error.
      int sendHeartbeat();

      /// Send a single backpropagated message.
      int sendBackpropagatedMessage();

      /// Determine if this node is the recruiter.
      bool isRecruiter() const;

      /// Return a copy of our persistent identifier.
      persistent_node_id_t getPersistentID() const;

      /// Whether we're currently sending heartbeats.
      bool isHeartbeating;

      /// Whether the heartbeat-sending thread is currently active.
      bool isExpectingHeartbeats;

    private:
      /// Our persistent identifier.
      persistent_node_id_t ourPersistentID;

      node_id_t ourID;
      mutable pthread_mutex_t ourIDIsValid;

      /// The current Lamport time.
      // TODO(jloew2): Do we need a lock for this?
      lamp_time_t curTime;

      /// Our local copy of the membership list.
      MembershipList membershipList;

      /// Everything we've sent out (or figured out on our own) but haven't yet
      /// received a confirmation on. We keep track so we can resend it in the
      /// event of a dropped node or packet.
      changelist_t delta;
      mutable pthread_mutex_t deltaLock;

      /// Blocks until we have a valid ID.
      void waitForValidID() const;

      /// Update our internal clock. Pass zero to simply increment the clock.
      void updateTimestamp(const lamp_time_t newTime);

      /// Update our local membership list to reflect any new changes.
      void updateMembershipList(const changelist_t &updates);

      /// Remove any messages that we originally sent (in-place).
      void removeSentMessages(changelist_t &msg);
      void removeSentMessages_helper(std::list<node_id_t> &msgList,
                                     std::list<node_id_t> &deltaList);

      /// Add anything in our local delta to the changelist.
      void augmentWithDelta(changelist_t &msg);
      void augmentWithDelta_helper(std::list<node_id_t> &msgList,
                                   std::list<node_id_t> &deltaList);

      /// Conversions to/from network format.
      std::string convertChangelistToNetworkFormat(changelist_t msg) const;
      changelist_t convertNetworkFormatToChangelist(const std::string &msg) const;
  };
}
