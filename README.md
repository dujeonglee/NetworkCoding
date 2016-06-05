# NetworkCoding
  - Implementation of network coding data transportation.
  - Example code can be found in main.cpp.

# Why network coding transport
  - Provide reliable packet transport 
  - Guarantee packet order
  - Data is not stream but block, like UDP.

# Contributions
  - I welcome any contributions from you.

# Done
  - Thread-safe
  - Tx / Rx Multiplexing
  - Half-duplex communication

# TODO
  - Full duplex communication

#Example
  - Server
```
#include "ncserver.h"

ncserver nc_server(port, retransmission_timeout);
nc_server.open_session(client_ip, client_port, BLOCK_SIZE::SIZE8);
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
...
...
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
nc_server.send(client_ip, client_port, buffer, buffer_size, false);
/*Indicate that this is the last transmission by passing true for the last parameter*/
nc_server.send(client_ip, client_port, buffer, buffer_size, true);
```
  - Client
```
#include "ncclient.h"

void rx_callback(unsigned char* buff, unsigned int size){
  /*Do something with the data*/
}

ncclient nc_client(rx_port, rx_callback);
```
