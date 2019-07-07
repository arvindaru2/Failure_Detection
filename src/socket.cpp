#include <iostream>
#include <cerrno>
#include <fstream>
#include <cstring>
#include <sstream>
#include <vector>
#include <list>
#include <time.h>

#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>

#include <cstdlib>
#include <cstdio>

#include "socket.hpp"
#include "utils.hpp"


#define MAX_SIZE 512
#define TEST "5678"


char * generateServerHostname(persistent_node_id_t id)
{
  char * hostName = NULL;
  asprintf(&hostName, "fa17-cs425-g18-%02d.cs.illinois.edu", id);
  // MPLOG("Debug: Generated hostname %s for id %d", hostName, id);
  return hostName;
}


int openReadSocket(char *portId)
{
  // Based off:
  // http://beej.us/guide/bgnet/output/html/multipage/syscalls.html#connect
  int status;
  struct addrinfo hints;
  struct addrinfo *ai_results, *servinfo;
  int sockfd = -1;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // don't care IPv4/v6
  hints.ai_socktype = SOCK_DGRAM; // UDP stream socket
  hints.ai_flags = AI_PASSIVE; // fill in my IP automatically

  status = getaddrinfo(NULL, portId, &hints, &servinfo);
  if (status != 0) {
   MPLOG("getaddrinfo error: %s", gai_strerror(status));
    exit(1);
  }

  for(ai_results = servinfo; ai_results != NULL; ai_results = ai_results->ai_next){   // loops through to get a correct output
     sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
     MPLOG("Error opening socket: %s", strerror(errno));
      continue;
    }
    break;  // if no error, break from the loop
  }

  if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
   MPLOG("Error on bind: %s", strerror(errno));
    exit(1);
  }

  MPLOG("Waiting for connection on port %s", portId);
  freeaddrinfo(servinfo);

  return sockfd;
}




std::string receiveData(int sockfd)
{
  int received;
  std::string retPacket = "";
  char buf[MAX_SIZE];
  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  struct sockaddr_storage their_addr;
  socklen_t addr_len = sizeof(their_addr);
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0)
  {
    perror("Error");
  }
  received = recvfrom(sockfd, buf, MAX_SIZE-1, 0, (struct sockaddr *)&their_addr, &addr_len);
  if(received < 0)
  {
    return retPacket;
  }
  else
  {
    int i = 0;

    while(1)
    {
      retPacket += buf[i];
      i++;
      if(buf[i] == 'T' && buf[i+1] == 'T' && buf[i+2] == 'T')
        break;
    }
    return retPacket;
  }
}


int Write(const char *hostname, char *portId, std::string &thePacket)
{

  thePacket += "TTT";
  int status;
  struct addrinfo hints;
  struct addrinfo *ai_results, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC; // don't care IPv4/v6
  hints.ai_socktype = SOCK_DGRAM; // UDP stream socket

  status = getaddrinfo(hostname, portId, &hints, &ai_results);  // sets up the struct in order to make connections
  if (status != 0) {
    MPLOG("getaddrinfo error: %s", gai_strerror(status));
    exit(1);
  }

  // Find the valid result entry.
  servinfo = ai_results; // we are preparing to loop through to get a sufficient struct
  int sockfd;
  // Open up a socket.
  for(ai_results = servinfo; ai_results != NULL; ai_results = ai_results->ai_next){   // loops through to get a correct output
     sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (sockfd == -1) {
     MPLOG("Error opening socket: %s", strerror(errno));
      continue;
    }
    break;  // if no error, break from the loop
  }

  if(ai_results == NULL){
    MPLOG("NO available socket: %s", strerror(errno));  // if we could not find any possible solutions, program exits
    pthread_exit(0);
  }


  const char * packet = thePacket.c_str();
  size_t pacLength = strlen(packet);
  int sent;
  sent = sendto(sockfd, packet, pacLength, 0, ai_results->ai_addr, ai_results->ai_addrlen);

  freeaddrinfo(ai_results); // free the memory

  if(sent <= 0)
  {
    return -1;
    std::cout<<"NOT SENT"<<"\n";
  }
  else
  {
    return sent;
  }
}
