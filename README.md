# Bot Swarm

The goal of this project was to design a simple base for a “Communications Node” for a distributed communications system on a local area network. Each node should work without any form of hardcoded IP address, port number or unique node identifier and should be able to coexist with other nodes on the same machine.

## Implementation

For this project, it was assumed that all of the communication nodes would be running on a Linux operating system. They are designed to run on Linux and utilize how the operating system will auto assign ports to programs that don’t care where they end up. It is also assumed that all machines on the network, or at least all of the machines running the communication node application, have valid IPv4 addresses. For the sake of simplicity, the idea of moving data around between the node was ignored and focus was put onto building communications node network protocol.

### Approach

The concepts that were used for this project were based on some of ideas that also went into the communicating of DHCP IP addresses to devices that have just come online. With DHCP, these new devices send out a network-wide broadcast, using the IP address of `255.255.255.255`, and a DHCP server responds with information about the network. After getting this information, the newly configured device checks its configuration by trying other network services like trying to get a hostname from the local DNS server. This idea of having a new node go out and discover other nodes, which in turn, informs it about the neighborhood was where I started with this project.

When a new node comes online, it will attempt to bind to a specific port. I chose port `50000` because it seemed like a good idea a the time. This port is used for reading incoming broadcasts from new nodes. If there are multiple nodes running on the same machine, all but the first node will fail to bind and the instead bind to port `0`, the operating system will then assign it to a random available port. The new node will then send out a broadcast to port `50000` and only nodes bound that port will respond with information about previously discovered nodes. Nodes that were not able to listen to this pre-defined port are still able to communicate with other nodes, but it is the job neighboring nodes to share the fact that they exist. This dynamic port approach allows for multiple nodes to coexist on same machine and still be able to interact with each other and other nodes within the network.

With this implementation, nodes are connectionless. Using UDP, as opposed to TCP, communication with its neighbors is connectionless and data simply “shows up” without having to accept incoming connections with the other nodes and keep alive the connection. This allows for much faster network construction and allows nodes to react faster to incoming traffic from other nodes. It’s the connectionless-ness that makes the nodes so simple to add functionality to without having to verify that the connection is still alive, like with TCP.

After a node receives information about another online node, it verifies that it is reachable and only saves it as one of its neighboring nodes if it gets a response. It also calculates the round trip time in this same transaction by putting the system time in the packet, which the target copies into the response and sends it back. When the node receives the response, it calculates the round trip time by taking the difference between the system time in the packet and the current system time.

Nodes do not retain a specific state and process packets as they received. Any kind of configuration information that is needed with a specific situation is put into the packet and is retained through the transactions that take place. Similar to the idea of browser sessions, a sequence of packets all will have a specific context in mind, allowing nodes to recover what they were doing last. If for some reason, the communication nodes were running on devices with a smaller physical memory capacity, not having to retain the exact state of the communication between nodes would save on overall memory consumption.


### Trade-offs

Having the nodes not remember their states means that all information about where they were last at would have to be stored in the packets sent around the network. This causes packets be larger and potential target on the network for a hacker or other malicious entity. It may also be possible to crash nodes by sending fake malformed packets to nodes, this could be prevented by having tighter checking on the packets by using checksums or other data integrity methods. This could be addressed with encryption and pre-shared key or any means of verifying that the packet was received from a node.

With this dynamic port approach, only the first communication node on a particular machine will be able to respond discovery broadcasts. However, this first node is able to inform other nodes about other instances running on the same machine as they also would have been discovered using the same process and configuration as nodes running on other machines.

## Requirements

* Standard Linux g++ compiler
* Standard Linux libraries
* Make

## Compiling

 0. Check out the repository.
 0. Change directory to the project directory.
 0. Call `make`.
 0. Verify that an executable called `swarm` was created.

## Execution

 0. Call `./swarm` on each machine that would be a member of the swarm.
