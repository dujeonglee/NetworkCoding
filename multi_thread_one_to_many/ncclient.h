#ifndef NCCLIENT_H_
#define NCCLIENT_H_
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <functional>
#include <thread>
#include <mutex>
#include "networkcodingheader.h"

struct DecodingBuffer
{
    bool empty;
    unsigned char* encode;
    unsigned char* decode;
};

struct PktBuffer
{
    bool delivered;
    unsigned char* buffer;
};

class ncclient
{
private:
    enum STATE: unsigned char
    {
        CLOSE = 0,
        OPEN
    };
    const sockaddr_in _DATA_ADDR;
    const BLOCK_SIZE _MAX_BLOCK_SIZE;

    STATE _state;
    int _socket;
    std::thread _rx_thread;
    bool _rx_thread_running;
    unsigned char _rx_buffer[1500];
    unsigned char _rank;
    PktBuffer* _buffer;
    unsigned short int _blk_seq;
    DecodingBuffer* _decoding_matrix;
    unsigned char _losses;
    std::function <void (unsigned char *, unsigned int length)> _receive_callback;
    std::mutex _lock;

public:
    ncclient(unsigned short int port, BLOCK_SIZE block_size);
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
    bool _handle_original_packet(const unsigned char * const pkt, int size);
    int _innovative(unsigned char* pkt);
    void _decode(unsigned char *pkt, int size);
    bool _handle_remedy_packet(unsigned char *pkt, int size);
    void _receive_handler();
    void _unroll_decode_2(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_4(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_8(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_16(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_32(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_64(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_128(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_256(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_512(unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_1024(unsigned char *output, unsigned short position, unsigned char row_index);
public:
    unsigned short int recv(unsigned char* pkt, unsigned short int pkt_size);
    bool open_client(std::function <void (unsigned char *, unsigned int length)> rx_handler);
    void close_client();
};
#endif
