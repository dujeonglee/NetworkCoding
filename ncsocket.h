#ifndef _NCSOCKET_H_
#define _NCSOCKET_H_
#include <atomic>
#include <thread>
#include "networkcodingheader.h"
#include "nctx.h"
#include "ncrx.h"

class ncsocket{
public:
    ncsocket(unsigned short int port, unsigned int tx_timeout_milli, unsigned int rx_timeout_milli, std::function <void (unsigned char* buffer, unsigned int length, sockaddr_in addr)> rx);
    ~ncsocket();
private:
    enum STATE : unsigned char{
        INIT_FAILURE,
        INIT_SUCCESS
    };
    STATE _state;
    int _socket;
    nctx* _nc_tx;
    ncrx* _nc_rx;
    std::atomic<bool> _is_rx_thread_running;
    std::thread _rx_thread;
    unsigned char _rx_buffer[ETHERNET_MTU];

public:
    bool open_session(unsigned int ip, unsigned short int port, BLOCK_SIZE block_size, unsigned int timeout);
    void close_session(unsigned int ip, unsigned short int port);
    int send(unsigned int ip, unsigned short int port, unsigned char* buff, unsigned int size, bool force_start_retransmission);
};

#endif
