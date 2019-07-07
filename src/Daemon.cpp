#include <cstring>
#include <sstream>
#include <pthread.h>
#include "Daemon.hpp"
#include "net_types.hpp"
#include "socket.hpp"
#include "utils.hpp"

static void * send_heartbeats_forever(void *void_daemon);
static void * receive_heartbeats_forever(void *void_daemon);
static void * receive_bp_messages_forever(void *void_daemon);

g18::Daemon::Daemon(const persistent_node_id_t persistentID)
: isHeartbeating(false), isExpectingHeartbeats(false),
ourPersistentID(persistentID), curTime(1), deltaLock(PTHREAD_MUTEX_INITIALIZER)
{
  memset(&ourID, 0, sizeof(ourID));
  ourIDIsValid = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&ourIDIsValid);
  delta = (changelist_t){
    .joined = std::list<node_id_t>(),
    .left = std::list<node_id_t>(),
    .failed = std::list<node_id_t>(),
    .timestamp = curTime
  };
  beginExpectingBackpropagatedMessages();
  MPLOG("Created daemon with ID %u", persistentID);
  joinGroup();
}

persistent_node_id_t g18::Daemon::getBackpropagationTarget()
{
  waitForValidID();
  // Might be all zeroes
  node_id_t predecessorMaybe = membershipList.predecessorOf(ourID);
  return predecessorMaybe.ip;
}

void g18::Daemon::handleReceivedHeartbeat(const std::string &hb)
{
  node_id_t senderID;
  const char *hbstr = hb.c_str();
  senderID.ip = atol(hbstr);
  while (*hbstr++ != ':') {
    // Keep going
  }
  senderID.timestamp = atol(hbstr);
  // TODO(jloew2): Switch to a timer-based system instead of using socket timeouts
  // Until then, nothing to do here
  (void)senderID;
}

void g18::Daemon::handleMissedHeartbeat()
{
  updateTimestamp(0);
  // Figure out who should've been sending us a heartbeat
  waitForValidID();
  if (!membershipList.hasPredecessor(ourID)) {
    MPLOG("Error: timed out on a heartbeat, but nobody should be sending us heartbeats");
    return;
  }
  node_id_t sender = membershipList.predecessorOf(ourID);
  // Mark the sender as dead
  // membershipList.nodeDidDie(sender);
  // MPLOG("Node %d timed out; marking as dead", sender.ip);
  pthread_mutex_lock(&deltaLock);
  //delta.failed.push_back(sender);
  pthread_mutex_unlock(&deltaLock);
  // Tell everyone that the sender has died
  sendBackpropagatedMessage();
}

void g18::Daemon::handleReceivedBackpropagationMessage(const std::string &bp)
{
  // Check if it's a new node trying to join at the recruiter or a regular BP
  // message
  if (bp.length() > 0 && bp[0] == '+') {
    handleNodeJoinRequest(bp);
    return;
  }
  // Regular BP message
  changelist_t msg = convertNetworkFormatToChangelist(bp);
  updateTimestamp(msg.timestamp);

  MPLOG("Debug: About to update membership list");
  updateMembershipList(msg);
  MPLOG("Finished updating membership list");

  pthread_mutex_lock(&deltaLock);
  removeSentMessages(msg);
  augmentWithDelta(msg);
  pthread_mutex_unlock(&deltaLock);

  // Pass it on
  sendBackpropagatedMessage();
}

void g18::Daemon::handleNodeJoinRequest(const std::string &bp)
{
  MPLOG("Debug: Got node join request: %s", bp.c_str());
  // Make sure we're actually the recruiter
  if (!isRecruiter()) {
    MPLOG("Warning: we're not the recruiter, but a node is asking us to join");
    return;
  }
  // Parse out the new node's ID
  const persistent_node_id_t newNodeID = atol(bp.c_str());
  // Update our timestamp
  updateTimestamp(0);
  // Add the new node to our changelist
  pthread_mutex_lock(&deltaLock);
  const node_id_t newNode = (node_id_t){
    .ip = newNodeID,
    .timestamp = curTime
  };
  delta.joined.push_back(newNode);
  pthread_mutex_unlock(&deltaLock);
  membershipList.nodeDidJoin(newNode);
  // Add ourself to our changelist
  waitForValidID();
  addToDelta(ourID, NODE_STATE_ONLINE);
  // Send out our changelist
  sendBackpropagatedMessage();
}

std::string g18::Daemon::generateMessageForHeartbeat() const
{
  waitForValidID();
  std::stringstream ourIDStr;
  ourIDStr << ourID.ip << ":" << ourID.timestamp;
  return ourIDStr.str();
}

std::string g18::Daemon::generateMessageForBackpropagation()
{
  // Update the timestamp and return our delta list.
  pthread_mutex_lock(&deltaLock);
  delta.timestamp = curTime;
  std::string msg = convertChangelistToNetworkFormat(delta);
  pthread_mutex_unlock(&deltaLock);
  return msg;
}

void g18::Daemon::addToDelta(const node_id_t &node, const node_state_e state)
{
  // Choose the right list
  std::list<node_id_t> *list = NULL;
  switch (state) {
  case NODE_STATE_ONLINE:
    list = &delta.joined;
    break;
  case NODE_STATE_DEPARTED:
    list = &delta.left;
    break;
  case NODE_STATE_DIED:
    list = &delta.failed;
    break;
  }

  pthread_mutex_lock(&deltaLock);
  // Check if this node is already in the list
  for (auto it = list->begin(); it != list->end(); ++it) {
    if (isEqual(*it, node)) {
      // Already present, ignore this duplicate entry
      pthread_mutex_unlock(&deltaLock);
      return;
    }
  }
  // Not found; add it
  list->push_back(node);
  pthread_mutex_unlock(&deltaLock);
}

void g18::Daemon::joinGroup()
{
  if (isRecruiter()) {
    // I AM the group
    // We have not yet set our ID. Let's set it right meow.
    ourID = (node_id_t){
      .ip = ourPersistentID,
      .timestamp = curTime
    };
    membershipList.nodeDidJoin(ourID);
    pthread_mutex_unlock(&ourIDIsValid);
    beginExpectingHeartbeats();
    beginHeartbeating();
    MPLOG("Debug: Self-joined because we're the recruiter");
    return;
  }
  // Generate a message with just our persistent ID
  std::stringstream sstr;
  sstr << '+' << ourPersistentID;
  std::string joinMsg = sstr.str();
  // Send it to the recruiter
  char *recruiter = generateServerHostname(recruiterID);
  int err = Write(recruiter, BACK_PROP_PORT_STR, joinMsg);
  if (err == 0) {
    MPLOG("Error sending join message. Exiting");
    exit(1);
  }
  MPLOG("Debug: Sending join request");
  // And now we wait
}

void g18::Daemon::leaveGroup()
{
  // Wait until we have a persistent ID before we can leave
  waitForValidID();

  addToDelta(ourID, NODE_STATE_DEPARTED);

  // Send a message that we're leaving and kill ourself
  int err = sendBackpropagatedMessage();
  if (err != 0) {
    MPLOG("Error sending leave message");
    exit(1);
  }
  MPLOG("Sent leave message; goodbye");
  exit(0);
}

void g18::Daemon::killSelf() const
{
  MPLOG("Received orders to kill ourself. Complying.");
  exit(0);
}

void g18::Daemon::beginHeartbeating() const
{
  if (isHeartbeating) {
    MPLOG("Debug: Will NOT begin heartbeating");
    return; // Nothing to do
  }
  pthread_t tid;
  int err = pthread_create(&tid, NULL, send_heartbeats_forever, (void *)this);
  if (err != 0) {
    MPLOG("Error starting a new thread to send heartbeats: %s", strerror(errno));
  }
  MPLOG("Debug: Will begin heartbeating");
}

void g18::Daemon::beginExpectingHeartbeats() const
{
  if (isExpectingHeartbeats) {
    MPLOG("Debug: Will NOT begin expecting heartbeats");
    return; // Nothing to do
  }
  // Start listening for heartbeats on a new thread
  pthread_t tid;
  int err = pthread_create(&tid, NULL, receive_heartbeats_forever, (void *)this);
  if (err != 0) {
    MPLOG("Error starting a new thread to receive heartbeats: %s", strerror(errno));
  }
  MPLOG("Debug: Will begin expecting heartbeats");
}

void g18::Daemon::beginExpectingBackpropagatedMessages() const
{
  // Start listening for backpropagated messages on a new thread
  pthread_t tid;
  int err = pthread_create(&tid, NULL, receive_bp_messages_forever, (void *)this);
  if (err != 0) {
    MPLOG("Error starting a new thread to receive BP messages: %s", strerror(errno));
  }
  MPLOG("Debug: Will begin expecting BP messages");
}

int g18::Daemon::sendHeartbeat()
{
  // Figure out who to send the heartbeat to
  waitForValidID();
  if (!membershipList.hasSuccessor(ourID)) {
    // No node to which we can send a heartbeat
    return -1;
  }
  node_id_t successor = membershipList.successorOf(ourID);
  // Generate the heartbeat message
  std::string hb = generateMessageForHeartbeat();
  // Send the heartbeat
  char *recipientHostname = generateServerHostname(successor.ip);
  int err = Write(recipientHostname, FORWARD_PROP_PORT_STR, hb);
  free(recipientHostname);
  if (err == 0) {
    MPLOG("Error sending heartbeat");
    return -1;
  }
  MPLOG("Debug: Sent heartbeat");
  return 0;
}

int g18::Daemon::sendBackpropagatedMessage()
{
  // Make sure we actually have something to send
  pthread_mutex_lock(&deltaLock);
  if ((delta.joined.size() == 0) && (delta.left.size() == 0) &&
      (delta.failed.size() == 0)) {
    // Nothing to send
    MPLOG("Debug: Not sending BP message: nothing to send");
    pthread_mutex_unlock(&deltaLock);
    return 0;
  }
  pthread_mutex_unlock(&deltaLock);
  // Figure out who to send the backpropagated message to.
  waitForValidID();
  if (!membershipList.hasPredecessor(ourID)) {
    // No node to which we can send a backpropagated message
    MPLOG("Debug: No predecessor");
    return -1;
  }
  node_id_t recipient = membershipList.predecessorOf(ourID);
  // Generate the BP message
  std::string msg = generateMessageForBackpropagation();
  // Send the BP message
  char *recipientHostname = generateServerHostname(recipient.ip);
  MPLOG("Debug: Sending BP message %s to %s", msg.c_str(), recipientHostname);
  int err = Write(recipientHostname, BACK_PROP_PORT_STR, msg);
  free(recipientHostname);
  if (err == 0) {
    MPLOG("Error sending backpropagated message");
    return -1;
  }
  return 0;
}

bool g18::Daemon::isRecruiter() const
{
  return getPersistentID() == recruiterID;
}

persistent_node_id_t g18::Daemon::getPersistentID() const
{
  return ourPersistentID;
}

void g18::Daemon::waitForValidID() const
{
  pthread_mutex_lock(&ourIDIsValid);
  pthread_mutex_unlock(&ourIDIsValid);
}

void g18::Daemon::updateTimestamp(const lamp_time_t newTime)
{
  curTime = MAX(curTime, newTime) + 1;
}

void g18::Daemon::updateMembershipList(const changelist_t &updates)
{
  for (auto leftIter = updates.left.begin(); leftIter != updates.left.end(); ++leftIter) {
    membershipList.nodeDidLeave(*leftIter);
  }

  for (auto diedIter = updates.failed.begin(); diedIter != updates.failed.end(); ++diedIter) {
    membershipList.nodeDidDie(*diedIter);
  }

  for (auto joinIter = updates.joined.begin(); joinIter != updates.joined.end(); ++joinIter) {
    // MPLOG("Debug: Processing joined node");
    int nodeAddStatus = membershipList.nodeDidJoin(*joinIter);
    // Check if we've just been added, in which case we need to set our ID with
    // the timestamp.
    if (joinIter->ip == ourPersistentID) {
      if (pthread_mutex_trylock(&ourIDIsValid) != 0) {
        // We have not yet set our ID. Let's set it right meow.
        ourID = (node_id_t){
          .ip = ourPersistentID,
          .timestamp = curTime
        };
        MPLOG("Setting our ID to %02u%d", ourID.ip, ourID.timestamp);
        pthread_mutex_unlock(&ourIDIsValid);
        beginExpectingHeartbeats();
        beginHeartbeating();
      }
    } else if (nodeAddStatus > 0) {
      // A new node has joined; add ourself to the delta list as having joined
      waitForValidID();
      addToDelta(ourID, NODE_STATE_ONLINE);
    }
  }
}

void g18::Daemon::removeSentMessages(changelist_t &msg)
{
  removeSentMessages_helper(msg.joined, delta.joined);
  removeSentMessages_helper(msg.left, delta.left);
  removeSentMessages_helper(msg.failed, delta.failed);
}

void g18::Daemon::removeSentMessages_helper(std::list<node_id_t> &msgList,
                                            std::list<node_id_t> &deltaList)
{
  for (auto deltaIter = deltaList.begin(); deltaIter != deltaList.end(); ++deltaIter) {
    for (auto msgIter = msgList.begin(); msgIter != msgList.end(); ++msgIter) {
      if (isEqual(*deltaIter, *msgIter)) {
        // We're the original sender of this update. Remove it from both lists.
        deltaList.erase(deltaIter);
        msgList.erase(msgIter);
        break;
      }
    }
  }
}

void g18::Daemon::augmentWithDelta(changelist_t &msg)
{
  augmentWithDelta_helper(msg.joined, delta.joined);
  augmentWithDelta_helper(msg.left, delta.left);
  augmentWithDelta_helper(msg.failed, delta.failed);
}

void g18::Daemon::augmentWithDelta_helper(std::list<node_id_t> &msgList,
                                          std::list<node_id_t> &deltaList)
{
  for (auto deltaIter = deltaList.begin(); deltaIter != deltaList.end(); ++deltaIter) {
    bool found = false;
    for (auto msgIter = msgList.begin(); msgIter != msgList.end(); ++msgIter) {
      if (isEqual(*deltaIter, *msgIter)) {
        found = true;
        break;
      }
    }
    if (!found) {
      msgList.push_back(*deltaIter);
    }
  }
}

std::string g18::Daemon::convertChangelistToNetworkFormat(changelist_t theChanges) const
{
  std::string packet;
  std::stringstream theStream;
  theStream << theChanges.timestamp;
  int j, l, f;
  j = theChanges.joined.size();
  l = theChanges.left.size();
  f = theChanges.failed.size();
  node_id_t val;
  theStream << "j";
  for(int j1 = 0; j1 < j; j1++)
  {
    val = theChanges.joined.front();
    if(val.ip == 10)
    {
      theStream << 10;
    }
    else
    {
      theStream << 0;
      theStream << val.ip;
    }
    theStream << val.timestamp;
    theStream << "m";
    theChanges.joined.pop_front();
  }
  theStream << "l";
  for(int l1 = 0; l1 < l; l1++)
  {
    val = theChanges.left.front();
    if(val.ip == 10)
    {
      theStream << 10;
    }
    else
    {
      theStream << 0;
      theStream << val.ip;
    }
    theStream << val.timestamp;
    theStream << "m";
    theChanges.left.pop_front();
  }
  theStream << "f";
  for(int f1 = 0; f1 < f; f1++)
  {
    val = theChanges.failed.front();
    if(val.ip == 10)
    {
      theStream << 10;
    }
    else
    {
      theStream << 0;
      theStream << val.ip;
    }
    theStream << val.timestamp;
    theStream << "m";
    theChanges.failed.pop_front();
  }
  packet = theStream.str();
  return packet;
}

changelist_t g18::Daemon::convertNetworkFormatToChangelist(const std::string &CLPacket) const
{
  std::string temp;
  changelist_t ret;
  node_id_t hold;
  int j = 0;
  int k;
  int val;
  int val2;
  for(int i = 0; CLPacket[i] != 'j'; i++)
  {
    temp += CLPacket[i];
    j = i;
  }
  std::istringstream iss (temp);
  iss >> val;
  ret.timestamp = val;
  j = j+2;

  while(CLPacket[j] != 'l')
  {
    temp = "";
    k = 0;
    for(int i = j; CLPacket[i] != 'm'; i++)
    {
      temp += CLPacket[i];
      j = i;
      k++;
      if(k ==2)
      {
        temp += " ";
      }
    }
    std::istringstream iss (temp);
    iss >> val;
    iss >> val2;
    hold.ip = val;
    hold.timestamp = val2;
    ret.joined.push_back(hold);

    j = j+2;
  }

  j++;

  while(CLPacket[j] != 'f')
  {
    temp = "";
    k = 0;
    for(int i = j; CLPacket[i] != 'm'; i++)
    {
      temp += CLPacket[i];
      j = i;
      k++;
      if(k ==2)
      {
        temp += " ";
      }
    }
    std::istringstream iss (temp);
    iss >> val;
    iss >> val2;
    hold.ip = val;
    hold.timestamp = val2;
    ret.left.push_back(hold);

    j = j+2;
  }

  j++;

  while(j < static_cast<signed>(CLPacket.size()))
  {
    temp = "";
    k = 0;
    for(int i = j; CLPacket[i] != 'm'; i++)
    {
      temp += CLPacket[i];
      j = i;
      k++;
      if(k ==2)
      {
        temp += " ";
      }
    }
    std::istringstream iss (temp);
    iss >> val;
    iss >> val2;
    hold.ip = val;
    hold.timestamp = val2;
    ret.failed.push_back(hold);

    j = j+2;
  }

  return ret;
}

static void * send_heartbeats_forever(void *void_daemon)
{
  g18::Daemon *daemon = static_cast<g18::Daemon *>(void_daemon);
  if (daemon == NULL) {
    MPLOG("Got a NULL daemon");
    return NULL;
  }
  daemon->isHeartbeating = true;
  // Send heartbeats until the end of time.
  int err;
  do {
    err = daemon->sendHeartbeat();
  } while (err == 0);
  // Error
  daemon->isHeartbeating = false;
  return NULL;
}

static void * receive_heartbeats_forever(void *void_daemon)
{
  g18::Daemon *daemon = static_cast<g18::Daemon *>(void_daemon);
  if (daemon == NULL) {
    MPLOG("Got a NULL daemon");
    return NULL;
  }
  daemon->isExpectingHeartbeats = true;
  // Open a socket to receive heartbeats
  int sockfd = openReadSocket(FORWARD_PROP_PORT_STR);
  if (sockfd < 0) {
    MPLOG("Error opening heartbeat receive socket");
    exit(1);
  }
  // Receive heartbeats until the end of time.
  while (true) {
    std::string hb = receiveData(sockfd);
    if (hb.length() == 0) {
      // Socket timed out, somebody missed a heartbeat
      daemon->handleMissedHeartbeat();
    } else {
      // Got a heartbeat
      MPLOG("Got heartbeat: %s", hb.c_str());
      daemon->handleReceivedHeartbeat(hb);
    }
  }
  daemon->isExpectingHeartbeats = false;
}

static void * receive_bp_messages_forever(void *void_daemon)
{
  g18::Daemon *daemon = static_cast<g18::Daemon *>(void_daemon);
  if (daemon == NULL) {
    MPLOG("Got a NULL daemon");
    return NULL;
  }
  // Open a socket to receive BP messages
  int sockfd = openReadSocket(BACK_PROP_PORT_STR);
  if (sockfd < 0) {
    MPLOG("Error opening BP message receive socket");
    exit(1);
  }
  // Receive BP messages until the end of time.
  while (true) {
    std::string bp = receiveData(sockfd);
    if (bp.length() == 0) {
      continue; // FISI
    }
    MPLOG("Got BP message: %s", bp.c_str());
    daemon->handleReceivedBackpropagationMessage(bp);
  }
}
