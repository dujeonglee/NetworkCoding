# This repo is deprecated by NetworkCodingRev.
  - https://github.com/dujeonglee/NetworkCodingRev
# NetworkCoding
  - Implementation of network coding data transportation.
  - Example code can be found in main.cpp.

# Why network coding transport
  - Provide reliable packet transport 
  - Guarantee packet order
  - Data is not stream but block, like UDP.

# Contributions
  - I welcome any contributions from you.

# Features supported
  - Thread-safe
  - Full-duplex reliable data communication. (The packet order is preserved)
  - Full-duplex best-effort communication with constant redundancy for multimedia service. (The packet order is preserved)
  - Connectivity check.
  - Change network coding session parameters, e.g., block size, redundancy, retransmission interval, and etc, after "open_session"

# Features under developing
  - Congestion control for retransmission.
  - Extention to multicast transmission.

#Example
```C++
#include "ncsocket.h"

void rx_callback(unsigned char* buff, unsigned int size, sockaddr_in sender){
  /*Do something with the data*/
}

// Reliable transmission
{
  ncsocket nc_socket(port, tx_timeout, rx_timeout, rx_callback);
  nc_socket.open_session(client_ip, client_port, BLOCK_SIZE::SIZE8, retransmission_interval);
  nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
  /*
   * Retransmission is triggered every BLOCK_SIZE of packets are transmitted. 
   * If you want to immediately trigger retransmission pass "true" for the last argument as shown in below.
   */
  nc_socket.send(client_ip, client_port, buffer, buffer_size, true);
}

// Best-effort transmission with constant redundancy
{
  ncsocket nc_socket(port, tx_timeout, rx_timeout, rx_callback);
  nc_socket.open_session(client_ip, client_port, BLOCK_SIZE::SIZE8, retransmission_interval, 4 /*Number of redundant packets*/);
  nc_socket.send(client_ip, client_port, buffer, buffer_size, false);
  /*
   * 4 redundant packets are transmitted after all BLOCK_SIZE of packets are sent. 
   * If you want to immediately send redundant packets pass "true" for the last argument as shown in below.
   */
  nc_socket.send(client_ip, client_port, buffer, buffer_size, true);
}

// Make sure if remote host is ready to receive packet
{
  ncsocket nc_socket(port, tx_timeout, rx_timeout, rx_callback);
  nc_socket.open_session(client_ip, client_port, BLOCK_SIZE::SIZE8, retransmission_interval, 4 /*Number of redundant packets*/);
  if(nc_socket.connect_session(client_ip, client_port, 3, 500) == true)
  {
    // Remote host is ready.
  }
  else
  {
    // The host is not ready.
  }
}
```
