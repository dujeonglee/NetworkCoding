#ifndef NCSERVER_H_
#define NCSERVER_H_
#include "singleshottimer.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

class ncserver{
private:
    int _socket;
    const sockaddr_in _CTRL_ADDR;
    const sockaddr_in _DATA_ADDR;
    const unsigned char _MAX_BLOCK_SIZE;
    const unsigned int _TX_TIMEOUT;
	std::function <void (void)> _cancel_callback;
	std::function <void (void)> _timeout_callback;
    unsigned char _tx_cnt;
    unsigned short int _largest_pkt_size;
    unsigned short int _blk_seq;
    unsigned int _pkt_seq;
    unsigned char _redundant_pkts;
    unsigned char** _buffer;
	unsigned char* _rand_coef;
	unsigned char* _remedy_pkt;
    singleshottimer _timer;
	std::mutex _lock;

public:
    ncserver(unsigned int client_ip, unsigned short int port, unsigned char block_size, unsigned int timeout);
    ~ncserver();

private:
    sockaddr_in _build_addr(unsigned int ip, unsigned short int port);
public:
    bool open_server();
    unsigned short int send(unsigned char* pkt, unsigned short int pkt_size);
    void close_server();
};


#endif
