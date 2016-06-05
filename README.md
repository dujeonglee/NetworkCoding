# NetworkCoding
  - Implementation of network coding data transportation.
  - Example code can be found in main.cpp.

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
nc_server.send(client_ip, client_port, buffer, buffer_size);
```
  - Client
```
#include "ncclient.h"

void rx_callback(unsigned char* buff, unsigned int size){
  /*Do something with the data*/
}

ncclient nc_client(rx_port, rx_callback);
```
