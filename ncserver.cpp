#include "ncserver.h"
#include "networkcodingheader.h"
#include "finite_field.h"
#include <exception>
#include <string.h>
#include <iostream>
#include <stdio.h>

/*
 * Constructor: Initialize const values and set _state as CLOSE.
 */
ncserver::ncserver(unsigned int client_ip, unsigned short int port, unsigned char max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, port)),\
    _DATA_ADDR(_build_addr(client_ip, port)), \
    _MAX_BLOCK_SIZE(max_block_size),\
    _TX_TIMEOUT(timeout)
{
    _state = ncserver::CLOSE;
}

ncserver::ncserver(unsigned short int svrport, unsigned int client_ip, unsigned short int cport, unsigned char max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, svrport)),\
    _DATA_ADDR(_build_addr(client_ip, cport)), \
    _MAX_BLOCK_SIZE(max_block_size),\
    _TX_TIMEOUT(timeout)
{
    _state = ncserver::CLOSE;
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
unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size)
{
    unsigned short int ret = 0;
    if(_lock.try_lock() == false)
    {
        // There is other threads accessing this object to send, retransmission, open or close.
        // This case is regarded as failure and an application should deal with this failure.
        return 0;
    }
    if(pkt_size > MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE))
    {
        // A single packet size is limitted to MAX_PAYLOAD_SIZE for the simplicity.
        // Support of large data packet is my future work.
        _lock.unlock();
        return 0;
    }
    if(_state == ncserver::CLOSE)
    {
        // ncserver is not properely initialized using open_server().
        _lock.unlock();
        return 0;
    }
    if(_largest_pkt_size < pkt_size)
    {
        // _largest_pkt_size maintains the largest packet size of data packets with the same block ID.
        // _largest_pkt_size is used to efficiently encode data packets.
        // Note: Packets with the same block ID will be encoded together to repair random packet losses.
        _largest_pkt_size = pkt_size;
    }

    // Fill-in header information
    GET_BLK_SEQ(_buffer[_tx_cnt]) = _blk_seq;
    GET_BLK_SIZE(_buffer[_tx_cnt]) = _MAX_BLOCK_SIZE;
    GET_FLAGS(_buffer[_tx_cnt]) = FLAGS_HAS_DATA;
    if(_tx_cnt == _MAX_BLOCK_SIZE-1)
    {
        GET_FLAGS(_buffer[_tx_cnt]) |= FLAGS_END_OF_BLK;
    }
    GET_SIZE(_buffer[_tx_cnt]) = pkt_size;
    GET_LAST_INDICATOR(_buffer[_tx_cnt]) = 1; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
    memset(GET_CODE(_buffer[_tx_cnt]), 0x0, _MAX_BLOCK_SIZE);
    GET_CODE(_buffer[_tx_cnt])[_tx_cnt] = 1;

    // Copy data into payload
    memcpy(GET_PAYLOAD(_buffer[_tx_cnt], _MAX_BLOCK_SIZE), pkt, pkt_size);

    // Send data packet
    if(sendto(_socket, _buffer[_tx_cnt], pkt_size + HEADER_SIZE(_MAX_BLOCK_SIZE), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == (int)(pkt_size + HEADER_SIZE(_MAX_BLOCK_SIZE)))
    {
        ret = pkt_size;
    }
    else
    {
        ret = 0;
    }
    if(_tx_cnt == 0)
    {
        // Start timer if this packet is the first packet in the block.
        _timer->start(_TX_TIMEOUT, [&](){this->_retransmission_handler();}, [&](){this->_retransmission_handler();});
    }
    if(_tx_cnt == _MAX_BLOCK_SIZE-1)
    {
        // If this packet is the last packe in the block, timer is stopped and ncserver immediately starts retransmission phase.
        _timer->cancel();
    }
    _tx_cnt++;
    _lock.unlock();
    return ret;
}

/*
 * Send remedy packets.
 */
bool ncserver::_send_remedy_pkt()
{
    if(_tx_cnt == 0)
    {
        // No packets are  sent to the client, therefore, no need for remedy packets.
        return false;
    }
    // Generate random cofficient.
    for(unsigned int code = 0 ; code < _MAX_BLOCK_SIZE ; code++)
    {
        _rand_coef[code] = 2+rand()%254;
    }

    // Fill-in header information
    GET_BLK_SEQ(_remedy_pkt) = _blk_seq;
    GET_BLK_SIZE(_remedy_pkt) = _MAX_BLOCK_SIZE;
    GET_FLAGS(_remedy_pkt) = FLAGS_END_OF_BLK | FLAGS_HAS_DATA;
    // Encode packets.
    for(unsigned int pkt = 0 ; pkt < _tx_cnt ; pkt++)
    {
        for(unsigned int position = OUTERHEADER_SIZE ; \
            position < HEADER_SIZE(_MAX_BLOCK_SIZE)+GET_SIZE(_buffer[pkt])/*NOTE: 0^x = 0*/ ; \
            position++)
        {
            (pkt == 0?_remedy_pkt[position] = FiniteField::instance()->mul(_buffer[pkt][position], _rand_coef[pkt]):_remedy_pkt[position] ^= FiniteField::instance()->mul(_buffer[pkt][position], _rand_coef[pkt]));
        }
    }
    return sendto(_socket, _remedy_pkt, HEADER_SIZE(_MAX_BLOCK_SIZE)+_largest_pkt_size, 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR))\
            == (int)(HEADER_SIZE(_MAX_BLOCK_SIZE)+_largest_pkt_size);
}

/*
 * Retransmission handler: Transmit remedy packets until ack is received from the client.
 */
void ncserver::_retransmission_handler()
{
    std::lock_guard<std::mutex> lock(_lock);
    if(_tx_cnt < _MAX_BLOCK_SIZE-1)
    {
        // In case of tx timeout, ncserver immediately send end of block with null data.
        GET_BLK_SEQ(_remedy_pkt) = _blk_seq;
        GET_BLK_SIZE(_remedy_pkt) = _MAX_BLOCK_SIZE;
        GET_FLAGS(_remedy_pkt) = FLAGS_END_OF_BLK;
        GET_SIZE(_remedy_pkt) = 0;
        GET_LAST_INDICATOR(_remedy_pkt) = 0;
        memset(GET_CODE(_remedy_pkt), 0x0, _MAX_BLOCK_SIZE);
        if(sendto(_socket, _remedy_pkt, HEADER_SIZE(_MAX_BLOCK_SIZE), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) != (int)HEADER_SIZE(_MAX_BLOCK_SIZE))
        {
            std::cout << "Failed to send remedy pkt\n";
        }
    }
    // transmit remedy packet until ack is received from the client.
    bool ack = false;
    while(ack == false)
    {
        unsigned short int blk_seq = 0;
        int ret = recvfrom(_socket, (void*)&blk_seq, sizeof(blk_seq), 0, NULL, NULL);
        if(ret == sizeof(blk_seq))
        {
            if(blk_seq == _blk_seq)
            {
                ack = true;
                std::cout <<"RECV ACK\n";
            }
            else // sync block sequence number and retransmit original packets again
            {
                _blk_seq = blk_seq;
                for(int i = 0 ; i < _tx_cnt ; i++)
                {
                    GET_BLK_SEQ(_buffer[i]) = _blk_seq;
                    sendto(_socket, _buffer[i], GET_SIZE(_buffer[i]) + HEADER_SIZE(_MAX_BLOCK_SIZE), \
                           0, \
                           (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR));
                }
                std::cout <<"SYNC BLOCK SEQUENCE\n";
            }
        }
        else
        {
            if(_send_remedy_pkt() == false)
            {
                std::cout << "Failed to send remedy pkt\n";
            }
            else
            {
                std::cout << "Remedy Code: "<<0+GET_CODE(_remedy_pkt)[0]<<" "<<0+GET_CODE(_remedy_pkt)[1]<<" "<<0+GET_CODE(_remedy_pkt)[2]<<" "<<0+ GET_CODE(_remedy_pkt)[3]<<" "<<0+GET_CODE(_remedy_pkt)[4]<<std::endl;
            }
        }
    }
    _blk_seq++;
    if(_blk_seq == 0)
    {
        _blk_seq = 1;
    }
    _largest_pkt_size = 0;
    _tx_cnt = 0;
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
    const timeval tv = {0, 30};
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
            _buffer[buffer_index] = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
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
        _remedy_pkt = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
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

    try
    {
        _timer = new singleshottimer();
    }
    catch(std::exception &ex)
    {
        delete [] _rand_coef;
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
    _blk_seq = 1;

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
    delete _timer;
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
