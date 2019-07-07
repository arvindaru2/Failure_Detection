#pragma once
#include "net_types.hpp"

/// Generate the hostname of a machine from its persistent identifier.
char * generateServerHostname(persistent_node_id_t id);

/// Open a socket for receiving and return it as a file descriptor.
int openReadSocket(char *portId);

/// Receive data with a timeout.
std::string receiveData(int sockfd);

/// Returns 0 on success.
int Write(const char *hostname, char *portId, std::string &thePacket);
