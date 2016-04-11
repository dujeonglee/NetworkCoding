#ifndef NCCLIENT_H_
#define NCCLIENT_H_
#include "networkcodingheader.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <functional>
#include <thread>
#include <mutex>

class ncclient
{
private:
    enum STATE: unsigned char
    {
        CLOSE = 0,
        OPEN
    };
    const sockaddr_in _DATA_ADDR;
    const unsigned char _MAX_BLOCK_SIZE;

    STATE _state;
    int _socket;
    std::thread _rx_thread;
    bool _rx_thread_running;
    unsigned char _rx_buffer[1500];
    unsigned char _rank;
    unsigned char** _buffer;
    unsigned short int _blk_seq;
    std::function <void (unsigned char *, unsigned int length)> _receive_callback;
    std::mutex _lock;

public:
    ncclient(unsigned short int port, unsigned char block_size);
    ~ncclient();

private:
    inline sockaddr_in _build_addr(unsigned int ip, unsigned short int port)
    {
        sockaddr_in ret = {0};
        ret.sin_family = AF_INET;
        ret.sin_addr.s_addr = htonl(ip);
        ret.sin_port = htons(port);
        return ret;
    }
    void _receive_handler();
public:
    unsigned short int recv(unsigned char* pkt, unsigned short int pkt_size);
    bool open_client(std::function <void (unsigned char *, unsigned int length)> rx_handler);
    void close_client();
};
#endif
