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
class rx_session_info{
    friend class ncrx;
public:
    /**
     * @brief The STATE enum: Indicate if the resource is successfully allocated
     */
    enum STATE: unsigned char
    {
        INIT_FAILURE, // Resource allocation failure
        INIT_SUCCESS// Resource allocation is success
    };
    /**
     * @brief _state: Resource allocation state
     * Either INIT_FAILURE or INIT_SUCCESS
     */
    rx_session_info::STATE _state;
    /**
     * @brief _ADDR: Sender address
     */
    const sockaddr_in _ADDR;
    /**
     * @brief _MAX_BLOCK_SIZE: The maximum block size for network coding
     */
    const unsigned char _MAX_BLOCK_SIZE;
    /**
     * @brief _rank: The rank of the packets in "_buffer"
     */
    unsigned char _rank;
    /**
     * @brief _buffer: Packet buffer
     */
    PktBuffer* _buffer;
    /**
     * @brief _decoding_buffer: Buffer to store a decoded packet for receive callback function
     */
    NetworkCodingPktBuffer _decoding_buffer;
    /**
     * @brief _blk_seq: Current block sequence number
     */
    unsigned short int _blk_seq;
    /**
     * @brief _decoding_matrix: Decoding codes are generated from encoding codes by "_decode" function
     */
    DecodingBuffer* _decoding_matrix;
    /**
     * @brief _losses: The number of loss packets in a block
     */
    unsigned char _losses;
    /**
     * @brief server_session_info: Constructor of server_session_info
     * @param addr: remote host's address (host byte order)
     * @param blk_size: The value for "_MAX_BLOCK_SIZE"
     */
    rx_session_info(sockaddr_in addr, unsigned char blk_size);
    /**
     * @brief ~server_session_info
     */
    ~rx_session_info();
};

class ncrx
{
    friend class ncsocket;
private:
    enum STATE: unsigned char
    {
        INIT_FAILURE = 0,
        INIT_SUCCESS
    };
    const int _SOCKET;
    const std::function <void (unsigned char* buffer, unsigned int length, sockaddr_in addr)> _receive_callback;
    avltree<ip_port_key, rx_session_info*> _server_session_info;
    ncrx(int socket, std::function <void (unsigned char* buffer, unsigned int length, sockaddr_in addr)> rx);
    ~ncrx();
    void _rx_handler(unsigned char* buffer, unsigned int size, sockaddr_in* sender_addr, unsigned int sender_addr_len);
    bool _handle_original_packet(rx_session_info* const session_info, const unsigned char * const pkt, int size);
    int _innovative(rx_session_info * const session_info, unsigned char* pkt);
    void _decode(rx_session_info * const session_info, unsigned char *pkt, int size);
    bool _handle_remedy_packet(rx_session_info * const session_info, unsigned char *pkt, int size);
    void _unroll_decode_2(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_4(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_8(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_16(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_32(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_64(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_128(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_256(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_512(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
    void _unroll_decode_1024(rx_session_info * const session_info, unsigned char *output, unsigned short position, unsigned char row_index);
};
#endif
