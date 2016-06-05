#ifndef NCCLIENT_H_
#define NCCLIENT_H_
/*=========== C Header ===========*/
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
/*========== C++ Header ==========*/
#include <functional>
#include <iostream>
#include <mutex>
#include <thread>
/*========= Project Header =========*/
#include "avltree.h"
#include "finite_field.h"
#include "networkcodingheader.h"

/**
 * @brief The DecodingBuffer: Maintain encoding and decoding information.
 */
struct DecodingBuffer
{
    bool empty;
    unsigned char* encode;
    unsigned char* decode;
};

/**
 * @brief The PktBuffer struct: Maintain received packets.
 */
struct PktBuffer
{
    bool delivered;
    NetworkCodingPktBuffer pkt;
};

/**
 * @brief The server_session_info class:
 */
class server_session_info{
    friend class ncclient;
public:
    enum STATE: unsigned char
    {
        INIT_FAILURE,
        INIT_SUCCESS
    };
    server_session_info::STATE _state;
    const unsigned char _MAX_BLOCK_SIZE;
    unsigned char _rank;
    PktBuffer* _buffer;
    unsigned short int _blk_seq;
    DecodingBuffer* _decoding_matrix;
    unsigned char _losses;
    std::mutex _lock;
    server_session_info(unsigned char blk_size);
    ~server_session_info();
};

class ncclient
{
private:
    enum STATE: unsigned char
    {
        INIT_FAILURE = 0,
        INIT_SUCCESS
    };
    const sockaddr_in _DATA_ADDR;

    ncclient::STATE _state;
    int _socket;
    std::thread _rx_thread;
    bool _rx_thread_running;
    unsigned char _rx_buffer[1500];
    const std::function <void (unsigned char *, unsigned int length)> _receive_callback;
    avltree<ip_port_key, server_session_info*> _server_session_info;

public:
    ncclient(unsigned short int port, std::function <void (unsigned char *, unsigned int length)> rx);
    ~ncclient();

private:
    bool _handle_original_packet(server_session_info* const session_info, const unsigned char * const pkt, int size);
    int _innovative(server_session_info * const session_info, unsigned char* pkt);
    void _decode(server_session_info * const session_info, unsigned char *pkt, int size);
    bool _handle_remedy_packet(server_session_info * const session_info, unsigned char *pkt, int size);
    void _receive_handler();
    void _unroll_decode_2(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_4(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_8(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_16(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_32(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_64(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_128(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_256(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_512(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_1024(server_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
public:
    unsigned short int recv(unsigned char* pkt, unsigned short int pkt_size);
    bool open_client(std::function <void (unsigned char *, unsigned int length)> rx_handler);
    void close_client();
};
#endif
