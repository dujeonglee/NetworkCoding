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
  - Full duplex communication

# TODO
  - Session close and open logic for robust operation
  - Multicast error control

#Example
```C++
#include "ncsocket.h"

void rx_callback(unsigned char* buff, unsigned int size, sockaddr_in sender){
  /*Do something with the data*/
}

ncsocket nc_socket(port, tx_timeout, rx_timeout, rx_callback);
nc_socket.open_session(client_ip, client_port, BLOCK_SIZE::SIZE8, retransmission_interval);
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
...
...
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
/*Indicate that this is the last transmission by passing true for the last parameter*/
nc_socket.send(client_ip, client_port, buffer, buffer_size, true);
```
