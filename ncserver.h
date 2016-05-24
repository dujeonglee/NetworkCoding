#ifndef NCSERVER_H_
#define NCSERVER_H_
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <mutex>
#include "networkcodingheader.h"

class ncserver
{
private:
    enum STATE: unsigned char
    {
        CLOSE = 0,
        OPEN
    };

    /*
     * Control channel address
     */
    const sockaddr_in _CTRL_ADDR;
    /*
     * Data channel address
     */
    const sockaddr_in _DATA_ADDR;
    /*
     * Maximum number of packets in a network coding block.
     */
    const unsigned char _MAX_BLOCK_SIZE;

    const unsigned int _TIMEOUT;

    /*
     * State of network coding server. Either CLOSE or OPEN.
     */
    STATE _state;

    /*
     * Socket descriptor for control and data channel
     */
    int _socket;

    /*
     * Array of packets in the network coding block
     */
    unsigned char** _buffer;
    /*
     * Random coefficients
     */
	unsigned char* _rand_coef;
    /*
     * Buffer for remedy packet
     */
	unsigned char* _remedy_pkt;
    /*
     * Number of packets transmitted for this block
     */
    unsigned char _tx_cnt;
    /*
     * The largest packet size in this block
     */
    unsigned short int _largest_pkt_size;
    /*
     * Server sequence
     */
    unsigned short int _blk_seq;
    /*
     *
     */
    float _loss_rate;
    /*
     * Lock
     */
    std::mutex _lock;

public:
    ncserver(unsigned int client_ip, unsigned short int port, BLOCK_SIZE block_size, unsigned int timeout);
    ncserver(unsigned short int svrport, unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size, unsigned int timeout);
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

public:
    unsigned short int send(unsigned char* pkt, unsigned short int pkt_size, const bool complete_block);
    bool open_server();
    void close_server();
};

#endif
