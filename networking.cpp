// Copyright [2016] Ian Wakely <raveious.irw@gmail.com>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <time.h>
#include "./networking.h"

NodeComm::NodeComm(void) {
  int enabled = 1;
  int resp = 0;
  struct sockaddr_in sock_config;
  socklen_t sock_config_len = sizeof(struct sockaddr_in);

  memset(&self, 0, sizeof(node_t));
  memset(&sock_config, 0, sizeof(struct sockaddr_in));

  sockfd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sockfd == -1) {
    perror("Socket failed to be created");
  }

  // Enable UDP broadcasts, we're going to need those...
  resp = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &enabled, sizeof(int));

  if (resp == -1) {
    perror("Failed to configure socket");
  }

  sock_config.sin_addr.s_addr = INADDR_ANY;
  sock_config.sin_port = htons(BROADCAST_PORT);
  sock_config.sin_family = PF_INET;

  // Try to bind to one port, but pick some other one if that fails
  resp = bind(sockfd, (struct sockaddr *)&sock_config, sock_config_len);
  if (resp == -1) {
    if (errno == EADDRINUSE) {
      sock_config.sin_port = htons(0);
      resp = bind(sockfd, (struct sockaddr *)&sock_config, sock_config_len);
      if (resp == -1) {
        perror("Failed to bind to random port");
      }
    } else {
      perror("Failed to bind");
    }
  }

  // TODO use real network IP, not the one from the socket config
  resp = getsockname(sockfd, (struct sockaddr *)&sock_config, &sock_config_len);

  if (resp == -1) {
    perror("Failed to get this node's network configuration");
  }

  // save the local information for sharing it with others
  self.sock_info = sock_config;
  self.sock_info_len = sock_config_len;

  // Use the time as a seed
  srand(time(NULL));

  // generate a random UID
  int len = sizeof(self.uid);
  const char alphanum[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  // go char by char, cause why not
  for (int i = 0; i < len; ++i) {
      self.uid[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  // NULL terminate the UID string
  self.uid[len] = '\0';

  neighbors = 0;
  max_neighbors = 3;
  neighborhood = reinterpret_cast<node_t *>(malloc(
                                            sizeof(node_t) * max_neighbors));

  if (neighborhood == NULL) {
    perror("Failed to allocate neighborhood memory");
  }
}

NodeComm::~NodeComm(void) {
  free(neighborhood);
  close(sockfd);
}

int NodeComm::recvData(int timeout, payload_t* payload, node_t* node) {
  struct pollfd fds;
  int resp = 0;

  // configure poll setup
  fds.fd = sockfd;
  fds.events = POLLIN;

  // do poll
  resp = poll(&fds, 1, timeout);

  // if nothing is ready, just leave
  if (resp == -1) {
    perror("Polling recv failed");
    return -1;
  } else if (resp == 0) {
    return 0;
  }

  // read any data that is ready
  resp = recvfrom(sockfd, payload, sizeof(payload_t), 0,
                 (struct sockaddr *)&(node->sock_info),
                                    &(node->sock_info_len));

  // char buff[16];
  // getIP(buff, 16, node);
  // printf("%i | %s:%i\n", payload->type, buff, getPort(node));

  if (resp < 0) {
    perror("Failed to read incoming data");
    return -1;
  }

  // Ignore all data that is comming from ourselves...
  if (strcmp(self.uid, payload->uid) == 0) {
    // printf("Got data from self...\n");
    return 0;
  }

  strncpy(node->uid, payload->uid, sizeof(node->uid));

  return payload->type;
}

void NodeComm::pushUpdate(void) {
  node_t broadcast = broadcastNode();
  pushUpdate(&broadcast);
}

void NodeComm::pushUpdate(node_t* node) {
  payload_t data;

  memset(&data, 0, sizeof(payload_t));

  data.type = UPDATE;
  getMyUID(data.uid);

  // send data about itself
  data.context = self;
  sendPayload(node, &data);

  // This node doesn't know any neighbors, respond only with info about itself
  if (neighbors > 0) {
    // but if we do know of neighbors, send those along
    for (int i = 0; i < neighbors; i++) {
      memcpy(&(data.context), &neighborhood[i], sizeof(node_t));
      printf("pushing info about \"%s\"\n", data.context.uid);
      sendPayload(node, &data);
    }
  }
}

int NodeComm::saveNewNode(node_t* node) {
  node_t* temp;
  // check to see if there is room available
  if (neighbors == max_neighbors - 1) {
    max_neighbors *= 2;

    // resize if needed
    temp = reinterpret_cast<node_t *>(realloc(neighborhood,
                                              sizeof(node_t) * max_neighbors));

    if (temp == NULL) {
      perror("Failed to make neighborhood bigger");
    } else {
      neighborhood = temp;
    }
  }

  memcpy(&neighborhood[neighbors], node, sizeof(node_t));
  neighbors++;

  return neighbors;
}

bool NodeComm::knownNode(node_t node) {
  for (int i = 0; i < neighbors; i++) {
    if (strcmp(node.uid, neighborhood[i].uid) == 0) {
      return true;
    }
  }

  return false;
}

void NodeComm::pingAll(void) {
  for (int i = 0; i < neighbors; i++) {
    ping(&neighborhood[i], NULL);
  }
}

void NodeComm::ping(node_t* node, time_t* timestamp) {
  time_t local_time = time(NULL);
  payload_t data;
  memset(&data, 0, sizeof(payload_t));

  getMyUID(data.uid);
  data.context = self;

  // sending a new ping, include our local time to get it back later
  if (timestamp == NULL) {
    data.type = PING;

    // use this timestamp because the one given is NULL
    timestamp = &local_time;
  } else {
    // this is a response to an existing ping
    data.type = PING_RESP;
  }

  // copy the timestamp of the other node back into the payload
  memcpy(data.data, timestamp, sizeof(clock_t));

  sendPayload(node, &data);
}

void NodeComm::shutdown(node_t* node) {
  // TODO Make a payload, use type SHUTDOWN and send it to node
}

void NodeComm::sendCommand(char* command) {
  // TODO make sending commands work
}

void NodeComm::discover(void) {
  payload_t payload;
  node_t broadcast = broadcastNode();

  memset(&payload, 0, sizeof(payload_t));
  getMyUID(payload.uid);
  payload.type = DISCOVERY;

  sendPayload(&broadcast, &payload);
}

int NodeComm::sendPayload(node_t* node, payload_t* payload) {
  // char buff[16];
  // getIP(buff, 16, node);
  // printf("Sending data to %s:%d\n", buff, getPort(node));

  // getIP(buff, 16, &payload->context);
  // printf("%i | %s:%i\n", payload->type, buff, getPort(&payload->context));

  return sendto(sockfd, payload, sizeof(payload_t), 0,
               (struct sockaddr *)&node->sock_info,
                                   node->sock_info_len);
}

void NodeComm::getIP(char* buff, int len) {
  getIP(buff, len, &self);
}

void NodeComm::getIP(char* buff, int len, node_t* node) {
  char* temp = inet_ntoa(node->sock_info.sin_addr);

  strncpy(buff, temp, len);
}

int NodeComm::getPort(void) {
  return getPort(&self);
}

int NodeComm::getPort(node_t* node) {
  return ntohs(node->sock_info.sin_port);
}

void NodeComm::getMyUID(char* buff) {
  strncpy(buff, self.uid, sizeof(self.uid));
}

node_t NodeComm::broadcastNode(void) {
  node_t broadcast;
  strncpy(broadcast.uid, "BROADCAST", sizeof(broadcast.uid));


  // TODO MAKE BROADCASTING WORK.......
  // broadcast.sock_info.sin_addr.s_addr = htonl(INADDR_BROADCAST);
  inet_aton("127.0.0.1", &broadcast.sock_info.sin_addr);
  broadcast.sock_info.sin_port = htons(BROADCAST_PORT);
  broadcast.sock_info.sin_family = PF_INET;

  broadcast.sock_info_len = sizeof(broadcast.sock_info);

  return broadcast;
}
