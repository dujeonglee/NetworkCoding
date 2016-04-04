#ifndef NCSERVER_H_
#define NCSERVER_H_
#include "singleshottimer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

class ncserver
{
private:
    const sockaddr_in _CTRL_ADDR;
    const sockaddr_in _DATA_ADDR;
    const unsigned char _MAX_BLOCK_SIZE;
    const unsigned int _TX_TIMEOUT;
    const std::function <void (void)> _CANCEL_CALLBACK;
    const std::function <void (void)> _TIMEOUT_CALLBACK;

    int _socket;
    unsigned char** _buffer;
	unsigned char* _rand_coef;
	unsigned char* _remedy_pkt;
    unsigned char _tx_cnt;
    unsigned short int _largest_pkt_size;
    unsigned short int _blk_seq;
    unsigned char _redundant_pkts;
    singleshottimer* _timer;
    bool _is_opned;

    std::mutex _lock;

public:
    ncserver(unsigned int client_ip, unsigned short int port, unsigned char block_size, unsigned int timeout);
    ~ncserver();

private:
    sockaddr_in _build_addr(unsigned int ip, unsigned short int port);
    std::function <void (void)> _buid_timer_callback();
public:
    bool open_server();
    unsigned short int send(unsigned char* pkt, unsigned short int pkt_size);
    void close_server();
};


#endif
