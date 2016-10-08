// Copyright [2016] Ian Wakely <raveious.irw@gmail.com>

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "./networking.h"

#define READ_TIMEOUT 2000
#define UPDATE_FREQ  10.0

NodeComm network = NodeComm();

void controlled_exit(int signal) {
  // TODO Alert other nodes that this one is going down

  printf("Executing a controlled exit.\n");
  exit(0);
}

int main(int argc, char const *argv[]) {
  int resp = 0;
  char temp[16];
  time_t timestamp;
  time_t delay;
  payload_t data;
  node_t incoming;

  // configure the action taken for a SIGINT event
  struct sigaction signalHandler;
  signalHandler.sa_handler = controlled_exit;
  sigemptyset(&signalHandler.sa_mask);
  signalHandler.sa_flags = 0;

  // setup to use the new handler
  sigaction(SIGINT, &signalHandler, NULL);

  memset(temp, 0, 16);
  memset(&data, 0, sizeof(payload_t));
  memset(&incoming, 0, sizeof(node_t));

  network.getMyUID(temp);
  printf("This node is \"%s\"", temp);

  network.getIP(temp, 16);
  printf(" on %s:%d\n", temp, network.getPort());

  // Tell other nodes that this node is alive
  printf("Trying to discover other nodes...\n");
  network.discover();

  delay = time(NULL);

  // event loop
  while ( 1 == 1 ) {
    // check for new data
    resp = network.recvData(READ_TIMEOUT, &data, &incoming);

    if (resp == DISCOVERY) {
      // A new node has sent a discovery packet
      network.getIP(temp, 16, &incoming);
      printf("Discovered \"%s\" @ %s:%d\n", incoming.uid, temp,
                                            network.getPort(&incoming));

      // tell the other node about all of the known nodes
      printf("Pushing update to \"%s\"\n", incoming.uid);
      network.pushUpdate(&incoming);

      network.saveNewNode(&incoming);
    } else if (resp == UPDATE) {
      /// recieved new data about potential nearby nodes
      printf("Recieved update from \"%s\"\n", incoming.uid);

      // check to see the information contains a node that is known about
      if (!network.knownNode(data.context)) {
        printf("\"%s\" is a potential new node, pinging target...\n",
               data.context.uid);

        // this is a potential new node
        network.ping(&data.context, NULL);
      }
    } else if (resp == REDIRECT) {
      // Data was sent to this node to redirect to another node
      // TODO add the ability to redirect data to another node
    } else if (resp == PING) {
      printf("Got pinged by \"%s\", responding...\n", incoming.uid);

      // get the other nodes clock value out of the payload
      memcpy(&timestamp, data.data, sizeof(clock_t));

      // Another node is checking if we're alive, just respond
      network.ping(&incoming, &timestamp);

      // check if that ping was from an existing node
      if (!network.knownNode(incoming)) {
        // save it if it was a new node
        network.saveNewNode(&incoming);
      }
    } else if (resp == PING_RESP) {
      // Got a response from the ping we sent

      // get the other nodes clock value out of the payload
      memcpy(&timestamp, data.data, sizeof(clock_t));
      timestamp = time(NULL) - timestamp;

      // Calculate the round trip time
      printf("RTT to \"%s\" is %.2fms\n", incoming.uid,
             static_cast<float>(timestamp));

      // check if that ping was from an existing node
      if (!network.knownNode(incoming)) {
        // save this node because it's good
        network.saveNewNode(&incoming);
      }
      // break;
    } else if (resp == SHUTDOWN) {
      // This node is instructed to shutdown
      raise(SIGINT);
    } else if (resp == COMMAND) {
      // This node is instructed to do something
      // TODO fork and exec a shell command
    }

    if ((time(NULL) - delay) >= UPDATE_FREQ) {
      printf("Checking to see who is still alive out there...\n");

      delay = time(NULL);

      network.pingAll();
    }
  }
  return 0;
}
