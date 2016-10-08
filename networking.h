// Copyright [2016] Ian Wakely <raveious.irw@gmail.com>

#ifndef _NETWORKING_H_
#define _NETWORKING_H_

#include <arpa/inet.h>

#define BROADCAST_PORT 50000

// different types of packets that can be sent around
#define DISCOVERY    1
#define UPDATE       2
#define REDIRECT     3
#define PING         4
#define PING_RESP    5
#define SHUTDOWN     6
#define COMMAND      7
#define COMMAND_RESP 8

typedef struct Node {
  char uid[10];
  struct sockaddr_in sock_info;
  socklen_t sock_info_len;
  bool aliased;
} node_t;

typedef struct Payload {
  char type;
  char uid[10];
  node_t context;
  char data[32];
} payload_t;

class NodeComm {
  int sockfd;
  node_t self;

  int neighbors;
  int max_neighbors;
  node_t* neighborhood;

 public:
    NodeComm(void);
    ~NodeComm(void);

    void discover(void);
    void sendCommand(char* command);
    void pushUpdate(void);
    void pushUpdate(node_t* node);
    void ping(node_t* node, time_t* timestamp);
    void pingAll(void);
    void shutdown(node_t* node);

    int recvData(int timeout, payload_t* payload, node_t* node);

    int saveNewNode(node_t* latest);

    void getMyUID(char* buff);

    void getIP(char* buff, int len);
    void getIP(char* buff, int len, node_t* node);

    int getPort(void);
    int getPort(node_t* node);

    bool knownNode(node_t node);

 private:
    int sendPayload(node_t* node, payload_t* payload);
    node_t broadcastNode(void);
};

#endif  // _NETWORKING_H_
