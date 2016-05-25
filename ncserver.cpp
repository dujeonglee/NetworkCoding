#include <string.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include "finite_field.h"
#include "ncserver.h"

/*
 * Constructor: Initialize const values and set _state as CLOSE.
 */
ncserver::ncserver(unsigned int client_ip, unsigned short int port, BLOCK_SIZE max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, port)),\
    _DATA_ADDR(_build_addr(client_ip, port)), \
    _MAX_BLOCK_SIZE(max_block_size),\
    _TIMEOUT(timeout)
{
    _state = ncserver::CLOSE;
    _socket = -1;
    _buffer = nullptr;
    _rand_coef = nullptr;
    _remedy_pkt = nullptr;
    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = rand();
}

ncserver::ncserver(unsigned short int svrport, unsigned int client_ip, unsigned short int cliport, BLOCK_SIZE max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, svrport)),\
    _DATA_ADDR(_build_addr(client_ip, cliport)), \
    _MAX_BLOCK_SIZE(max_block_size),\
    _TIMEOUT(timeout)
{
    _state = ncserver::CLOSE;
    _socket = -1;
    _buffer = nullptr;
    _rand_coef = nullptr;
    _remedy_pkt = nullptr;;
    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = rand();
}

/*
 * Destructor: Release resources by "close_server".
 */
ncserver::~ncserver()
{
    close_server();
}

/*
 * Send data to the client.
 * pkt: Pointer for data.
 * pkt_size: Size of data.
 */
unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size, const bool complete_block)
{
    std::lock_guard<std::mutex> lock(_lock);

    unsigned short int ret = 0;
    if(pkt_size > MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE))
    {
        // A single packet size is limitted to MAX_PAYLOAD_SIZE for the simplicity.
        // Support of large data packet is my future work.
        return 0;
    }
    if(_state == ncserver::CLOSE)
    {
        // ncserver is not properely initialized using open_server().
        return 0;
    }
    if(_largest_pkt_size < pkt_size)
    {
        // _largest_pkt_size maintains the largest packet size of data packets with the same block ID.
        // _largest_pkt_size is used to efficiently encode data packets.
        // Note: Packets with the same block ID will be encoded together to repair random packet losses.
        _largest_pkt_size = pkt_size;
    }
    // Fill outer header
    const unsigned char* pkt_buffer = _buffer[_tx_cnt];
    const bool invoke_retransmission = (_tx_cnt == _MAX_BLOCK_SIZE - 1) || (complete_block == true);

    GET_OUTER_BLK_SEQ(pkt_buffer) = _blk_seq;
    GET_OUTER_SIZE(pkt_buffer) = TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+pkt_size;
    GET_OUTER_BLK_SIZE(pkt_buffer) = _tx_cnt;
    GET_OUTER_FLAGS(pkt_buffer) = OuterHeader::FLAGS_ORIGINAL;
    if(invoke_retransmission)
    {
        GET_OUTER_FLAGS(pkt_buffer) |= OuterHeader::FLAGS_END_OF_BLK;
    }

    // Fill inner header
    GET_INNER_SIZE(pkt_buffer) = pkt_size;
    GET_INNER_LAST_INDICATOR(pkt_buffer) = 1; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
    memset(GET_INNER_CODE(pkt_buffer), 0x0, _MAX_BLOCK_SIZE);
    GET_INNER_CODE(pkt_buffer)[_tx_cnt] = 1; // Array index is 0 base.

    // Copy data into payload
    memcpy(GET_INNER_PAYLOAD(pkt_buffer, _MAX_BLOCK_SIZE), pkt, pkt_size);

    // Send data packet
    //PRINT_OUTERHEADER(pkt_buffer);
    //PRINT_INNERHEADER(_MAX_BLOCK_SIZE, pkt_buffer);
    if(sendto(_socket, pkt_buffer, GET_OUTER_SIZE(pkt_buffer), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == (int)GET_OUTER_SIZE(pkt_buffer))
    {
        ret = pkt_size;
    }
    else
    {
        ret = 0;
    }
    if(invoke_retransmission)
    {
        bool ack = false;
        for(unsigned char i = 0 ; i < (unsigned char)_loss_rate+1 ; i++)
        {
            _send_remedy_pkt();
        }
        while(ack == false)
        {
            Ack ack_pkt;
            int ret = recvfrom(_socket, (void*)&ack_pkt, sizeof(ack_pkt), 0, NULL, NULL);
            if(ret == sizeof(ack_pkt))
            {
                if(ack_pkt.blk_seq == _blk_seq)
                {
                    ack = true;
                    _loss_rate = (_loss_rate + (float)ack_pkt.losses)/2.;
                }
            }
            else
            {
                if(_send_remedy_pkt() == false)
                {
                    std::cout<<"Could not send remedy pkt\n";
                }
            }
        }
        unsigned short next_blk_seq;
        do
        {
            next_blk_seq = rand();
        }while(next_blk_seq == _blk_seq);
        _blk_seq = next_blk_seq;
        _largest_pkt_size = 0;
        _tx_cnt = 0;
    }
    else
    {
        _tx_cnt++;
    }
    return ret;
}

/*
 * Send remedy packets.
 */
bool ncserver::_send_remedy_pkt()
{
    if(_tx_cnt == 0)
    {
        return (int)GET_OUTER_SIZE(_buffer[0]) == sendto(_socket, _buffer[0], GET_OUTER_SIZE(_buffer[0]), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR));
    }
    memset(_remedy_pkt, 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
    // Generate random cofficient.
    for(unsigned int code_id = 0 ; code_id < _MAX_BLOCK_SIZE ; code_id++)
    {
        _rand_coef[code_id] = code_id <= _tx_cnt?1+rand()%255:0;
    }

    // Fill-in header information
    GET_OUTER_SIZE(_remedy_pkt) = TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+_largest_pkt_size;
    GET_OUTER_BLK_SEQ(_remedy_pkt) = _blk_seq;
    GET_OUTER_BLK_SIZE(_remedy_pkt) = _tx_cnt;
    GET_OUTER_FLAGS(_remedy_pkt) = OuterHeader::FLAGS_END_OF_BLK;
    // Encode packets.
    for(unsigned int packet_id = 0 ; packet_id <= _tx_cnt ; packet_id++)
    {
        for(unsigned int coding_position = OUTER_HEADER_SIZE ;
            coding_position < GET_OUTER_SIZE(_buffer[packet_id])/*NOTE: 0^x = 0*/ ;
            coding_position++)
        {
            _remedy_pkt[coding_position] ^= FiniteField::instance()->mul(_buffer[packet_id][coding_position], _rand_coef[packet_id]);
        }
    }
    return (int)GET_OUTER_SIZE(_remedy_pkt) == sendto(_socket, _remedy_pkt, GET_OUTER_SIZE(_remedy_pkt), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR));
}

bool ncserver::open_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    // Socket initialization
    if(_state == ncserver::OPEN)
    {
        return true;
    }
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1)
    {
        return false;
    }
    const int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }
    const timeval tv = {_TIMEOUT/1000, _TIMEOUT%1000};
    if(setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }
    if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }
    if(bind(_socket, (sockaddr*)&_CTRL_ADDR, sizeof(_CTRL_ADDR)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }

    if(connect(_socket, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }

    // Buffer memory allocation
    try
    {
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
    }
    catch(std::exception &ex)
    {
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    int buffer_index = 0;
    try
    {
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++)
        {
            _buffer[buffer_index] = new unsigned char[TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
        }
    }
    catch(std::exception &ex)
    {
        for(--buffer_index ; buffer_index > -1 ; buffer_index--)
        {
            delete [] _buffer[buffer_index];
        }
        delete []_buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }
    // Remedy packet memory allocation
    try
    {
        _remedy_pkt = new unsigned char[TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
    }
    catch(std::exception &ex)
    {
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++)
        {
            delete [] _buffer[buffer_index];
        }
        delete []_buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }
    // Coefficient memory allocation
    try
    {
        _rand_coef = new unsigned char[_MAX_BLOCK_SIZE];
    }
    catch(std::exception &ex)
    {
        delete [] _remedy_pkt;
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++)
        {
            delete [] _buffer[buffer_index];
        }
        delete []_buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = 0;
    _loss_rate = 0.;
    _state = ncserver::OPEN;
    return true;
}

void ncserver::close_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    if(_state == ncserver::CLOSE)
    {
        return;
    }
    delete [] _rand_coef;
    delete [] _remedy_pkt;
    for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
    {
        delete [] _buffer[i];
    }
    delete []_buffer;
    close(_socket);
    _socket = -1;
    _state = ncserver::CLOSE;
}
