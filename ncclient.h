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

class ncclient{
private:
    int _socket;
    const sockaddr_in _DATA_ADDR;
    const unsigned char _MAX_BLOCK_SIZE;
    std::thread _rx_thread;
    bool _rx_thread_running;
    bool _exit_rx_thread;
    unsigned char _rx_buffer[1500];
    unsigned char _rank;
    unsigned char** _buffer;
    unsigned short int _blk_seq;
    std::function <void (unsigned char *, unsigned int length)> _rx_handler;
    std::mutex _lock;

public:
    ncclient(unsigned short int port, unsigned char block_size);
    ~ncclient();

private:
    sockaddr_in _build_addr(unsigned int ip, unsigned short int port);
    static bool _is_original_pkt(unsigned char* pkt, unsigned int max_blk);
public:
    bool open_client(std::function <void (unsigned char *, unsigned int length)> rx_handler);
    unsigned short int recv(unsigned char* pkt, unsigned short int pkt_size);
    void close_client();
};
#endif
