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
    enum STATE{
        CLOSE = 0,
        OPEN
    };
    const sockaddr_in _CTRL_ADDR;
    const sockaddr_in _DATA_ADDR;
    const unsigned char _MAX_BLOCK_SIZE;
    const unsigned int _TX_TIMEOUT;

    STATE _state;
    int _socket;
    unsigned char** _buffer;
	unsigned char* _rand_coef;
	unsigned char* _remedy_pkt;
    unsigned char _tx_cnt;
    unsigned short int _largest_pkt_size;
    unsigned short int _blk_seq;
    singleshottimer* _timer;
    std::mutex _lock;

public:
    ncserver(unsigned int client_ip, unsigned short int port, unsigned char block_size, unsigned int timeout);
    ~ncserver();

private:
    inline sockaddr_in _build_addr(unsigned int ip, unsigned short int port)
    {
        sockaddr_in ret = {0};
        ret.sin_family = AF_INET;
        ret.sin_addr.s_addr = htonl(ip);
        ret.sin_port = htons(port);
        return ret;
    }

    bool _send_remedy_pkt();
    void _retransmission_handler();

public:
    unsigned short int send(unsigned char* pkt, unsigned short int pkt_size);
    bool open_server();
    void close_server();
};

#endif
