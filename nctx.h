#ifndef NCSERVER_H_
#define NCSERVER_H_
/*=========== C Header ===========*/
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
/*========== C++ Header ==========*/
#include <atomic>
#include <exception>
#include <iostream>
#include <mutex>
#include <thread>
/*========= Project Header =========*/
#include "avltree.h"
#include "finite_field.h"
#include "networkcodingheader.h"

// Client session information
class tx_session_info
{
    friend class nctx;
private:
    /**
     * @brief The STATE enum: Indicate if the resource is successfully allocated
     */
    enum STATE: unsigned char{
        INIT_FAILURE, // Resource allocation failure
        INIT_SUCCESS// Resource allocation is success
    };
    /**
     * @brief _state: Resource allocation state
     * Either INIT_FAILURE or INIT_SUCCESS
     */
    STATE   _state;
    /**
     * @brief _DATA_ADDR: Client address
     */
    const sockaddr_in _DATA_ADDR;
    /**
     * @brief _MAX_BLOCK_SIZE: Maximum number of packets in a network coding block
     */
    const BLOCK_SIZE _MAX_BLOCK_SIZE;
    /**
     * @brief _TIMEOUT: Rx and Tx timeout value.
     */
    const unsigned int _TIMEOUT;
    /**
     * @brief _buffer: Packet buffer
     */
    NetworkCodingPktBuffer* _buffer;
    /**
     * @brief _rand_coef: Random coefficient array
     * Each coefficient is greater than 0, e.g., (rand()%255)+1,
     * so that all of the original packets in "_buffer" are encoded for remedy packets.
     */
    unsigned char* _rand_coef;// Random coefficients
    /**
     * @brief _remedy_pkt: Buffer for a remedy packet.
     */
    NetworkCodingPktBuffer _remedy_pkt;
    /**
     * @brief _tx_cnt: Number of packets transmitted for this block
     */
    unsigned char _tx_cnt;
    /**
     * @brief _largest_pkt_size: The largest packet size in "_buffer"
     */
    unsigned short int _largest_pkt_size;
    /**
     * @brief _blk_seq: Random sequence number for each block.
     * It must be different from the previous network coding block sequence.
     */
    unsigned short int _blk_seq;
    /**
     * @brief _loss_rate: The number of remedy packets in the previous block.
     * The nc server will send this many remedy packets before waiting for an ack from the client.
     */
    std::atomic<unsigned char> _loss_rate;
    /**
     * @brief _redundancy: =0xff: Reliable transmission
     *                                    <0xff: Best effort transmission.
     */
    std::atomic<unsigned char> _redundancy;
    /**
     * @brief _retransmission_in_progress: Indicate if retransmission is in progress for the client.
     * It will be true until an ack is received from the client.
     * Send will be blocked until "_retransmission_in_progress" becomes false.
     */
    std::atomic<bool> _retransmission_in_progress;
    /**
     * @brief _lock: Prevent racing conditions.
     */
    std::mutex _lock;

    /**
     * @brief tx_session_info: Creator of client session information
     * @param client_ip: Client's ip address (host byte order)
     * @param cport: Client's port number (host byte order)
     * @param block_size: Maximum block size.
     */
    tx_session_info(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size, unsigned int timeout, unsigned char redundancy);
    /**
     * @brief ~tx_session_info: Destructor of client session information
     */
    ~tx_session_info();
};


class nctx
{
    friend class ncsocket;
private:
    /**
     * @brief _socket: Socket to communicate with clients
     * Server sends and receives data and acks on this socket.
     */
    const int _SOCKET;
    /**
     * @brief _tx_session_info: Hash table data structure maintaining a list of clients. (thread-safe)
     */
    avltree<ip_port_key, tx_session_info*> _tx_session_info;

    /**
     * @brief _send_remedy_pkt: Send remedy packet for client "info"
     * @param info: Client's tx_session_info for retransmission
     * @return: On success true. Otherwise, false.
     */
    bool _send_remedy_pkt(tx_session_info* const info);
    /**
     * @brief ncserver: Creator of ncserver
     * @param svrport: Server control port number that will be passed to "_CTRL_ADDR".
     * @param timeout: Tx / Rx timeout value that will be passed to "_TIMEOUT"
     */
    nctx(int socket);
    /**
     * @brief ~ncserver: Destructor of ncserver
     */
    ~nctx();
    /**
     * @brief open_session: Open a new session
     * @param client_ip: Client ip address (host byte order)
     * @param cport: Client port number (host byte order)
     * @param block_size: Maximum block size chosen from BLOCK_SIZE
     * @return: On success true. Otherwise, false
     */
    bool open_session(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size, unsigned int timeout, unsigned char redundancy);
    /**
     * @brief close_session: Close a client session.
     * @param client_ip: Client ip address that you want to close (host byte order)
     * @param cport: Client port number that you want to close (host byte order)
     */
    void close_session(unsigned int client_ip, unsigned short int cport);
    /**
     * @brief send: Send packets.
     * @param client_ip: Destination ip address (host byte order)
     * @param cport: Destination port number (host byte order)
     * @param pkt: Packet buffer pointer
     * @param pkt_size: Size of "pkt".
     * @param complete_block: To force retransmission. (In the last transmission, "complete_block" must be true.)
     * @return: The number of bytes sent.
     */
    unsigned short int send(unsigned int client_ip, unsigned short int cport, unsigned char* pkt, unsigned short int pkt_size, const bool complete_block);
    void _rx_handler(unsigned char* buffer, unsigned int size, sockaddr_in* sender_addr, unsigned int sender_addr_len);
};

#endif
