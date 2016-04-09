#include "ncserver.h"
#include "networkcodingheader.h"
#include "finite_field.h"
#include <exception>
#include <string.h>
#include <iostream>
#include <stdio.h>

ncserver::ncserver(unsigned int client_ip, unsigned short int port, unsigned char max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, port)),\
    _DATA_ADDR(_build_addr(client_ip, port)), \
    _MAX_BLOCK_SIZE(max_block_size),\
    _TX_TIMEOUT(timeout)
{
    _state = CLOSE;
}

ncserver::~ncserver()
{
    close_server();
}

unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size)
{
    unsigned short int ret = 0;
    if(_lock.try_lock() == false)
    {
        return 0;
    }
    if(pkt_size > MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE))
    {
        // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
        _lock.unlock();
        return 0;
    }
    if(_state == CLOSE)
    {
        _lock.unlock();
        return 0;
    }
    if(_largest_pkt_size < pkt_size)
    {
        _largest_pkt_size = pkt_size;
    }

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
    memcpy(GET_PAYLOAD(_buffer[_tx_cnt], _MAX_BLOCK_SIZE), pkt, pkt_size);

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
        _timer->start(_TX_TIMEOUT, [](){this->_retransmission_handler();}, [](){this->_retransmission_handler();});
    }
    if(_tx_cnt == _MAX_BLOCK_SIZE-1)
    {
        _timer->cancel();
    }
    _tx_cnt++;
    _lock.unlock();
    return ret;
}

bool ncserver::_send_remedy_pkt()
{
    if(_tx_cnt == 0)
    {
        return false;
    }
    for(unsigned int code = 0 ; code < _MAX_BLOCK_SIZE ; code++)
    {
        _rand_coef[code] = 1+rand()%255;
    }
    GET_BLK_SEQ(_remedy_pkt) = _blk_seq;
    GET_BLK_SIZE(_remedy_pkt) = _MAX_BLOCK_SIZE;
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

void ncserver::_retransmission_handler()
{
    std::lock_guard<std::mutex> lock(_lock);
    _state = REMEDY_TX;
    if(_tx_cnt < _MAX_BLOCK_SIZE-1)
    {
        GET_BLK_SEQ(_remedy_pkt) = _blk_seq;
        GET_BLK_SIZE(_remedy_pkt) = _MAX_BLOCK_SIZE;
        GET_FLAGS(_remedy_pkt) = END_OF_BLK;
        GET_SIZE(_remedy_pkt) = 0;
        GET_LAST_INDICATOR(_remedy_pkt) = 0;
        memset(GET_CODE(_remedy_pkt), 0x0, _MAX_BLOCK_SIZE);
        if(sendto(_socket, _remedy_pkt, HEADER_SIZE(_MAX_BLOCK_SIZE), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) != (int)HEADER_SIZE(_MAX_BLOCK_SIZE))
        {
            std::cout << "Failed to send remedy pkt\n";
        }
    }
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
    _state = ORIGINAL_TX;
    _blk_seq++;
    _largest_pkt_size = 0;
    _tx_cnt = 0;
}

bool ncserver::open_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    // Socket initialization
    if(_state == OPEN)
    {
        return true;
    }
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1)
    {
        return false;
    }
    int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(_socket);
        return false;
    }
    timeval tv = {0, 30};
    if(setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        return false;
    }
    if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        return false;
    }
    if(bind(_socket, (sockaddr*)&_CTRL_ADDR, sizeof(_CTRL_ADDR)) == -1)
    {
        close(_socket);
        return false;
    }

    // Buffer memory allocation
    try
    {
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
    }
    catch(std::exception &ex)
    {
        close(_socket);
        return false;
    }

    int i = 0;
    try
    {
        for(i = 0 ; i < (int)_MAX_BLOCK_SIZE ; i++)
        {
            _buffer[i] = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
        }
    }
    catch(std::exception &ex)
    {
        for(--i ; i > -1 ; i--)
        {
            delete [] _buffer[i];
        }
        delete []_buffer;
        close(_socket);
        return false;
    }
    // Remedy packet memory allocation
    try
    {
        _remedy_pkt = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
    }
    catch(std::exception &ex)
    {
        for(i = 0 ; i < (int)_MAX_BLOCK_SIZE ; i++)
        {
            delete [] _buffer[i];
        }
        delete []_buffer;
        close(_socket);
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
        for(i = 0 ; i < (int)_MAX_BLOCK_SIZE ; i++)
        {
            delete [] _buffer[i];
        }
        delete []_buffer;
        close(_socket);
        return false;
    }

    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = 0;

    try
    {
        _timer = new singleshottimer();
    }
    catch(std::exception &ex)
    {
        delete [] _rand_coef;
        delete [] _remedy_pkt;
        for(i = 0 ; i < (int)_MAX_BLOCK_SIZE ; i++)
        {
            delete [] _buffer[i];
        }
        delete []_buffer;
        close(_socket);
        return false;
    }
    _state = OPEN;
    return true;
}

void ncserver::close_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    // Socket initialization
    if(_state == CLOSE)
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
    _state = CLOSE;
}
