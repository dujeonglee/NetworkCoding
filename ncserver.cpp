#include "ncserver.h"
#include "networkcodingheader.h"
#include "finite_field.h"
#include <exception>
#include <string.h>
#include <iostream>
#include <stdio.h>

ncserver::ncserver(unsigned int client_ip, unsigned short int port, unsigned char max_block_size, unsigned int timeout):\
    _CTRL_ADDR(_build_addr(INADDR_ANY, port)), _DATA_ADDR(_build_addr(client_ip, port)), \
    _MAX_BLOCK_SIZE(max_block_size), _TX_TIMEOUT(timeout),\
    _CANCEL_CALLBACK(_buid_timer_callback()), _TIMEOUT_CALLBACK(_buid_timer_callback())
{
    _is_opned = false;
}

ncserver::~ncserver()
{
    close_server();
}

sockaddr_in ncserver::_build_addr(unsigned int ip, unsigned short int port)
{
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

std::function <void (void)> ncserver::_buid_timer_callback()
{
    /*
     * TODO: Parallelize the encoding process using OpenMP or thread pool.
     * NOTE: This function should be called by singleshottimer. Otherwise, its operation is undefied.
     */
    std::function <void (void)> ret = [&](){
        std::lock_guard<std::mutex> lock(_lock);

        int ret;
        for(unsigned int tx = 0 ; tx < _redundant_pkts ; tx++)
        {
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
            ret = sendto(_socket, _remedy_pkt, HEADER_SIZE(_MAX_BLOCK_SIZE)+_largest_pkt_size, 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR));
            if(ret == -1)
            {
                std::cout << "Failed to send remedy pkt\n";
            }
            else
            {
                std::cout << "Remedy Code: "<<0+GET_CODE(_remedy_pkt)[0]<<" "<<0+GET_CODE(_remedy_pkt)[1]<<" "<<0+GET_CODE(_remedy_pkt)[2]<<" "<<0+ GET_CODE(_remedy_pkt)[3]<<" "<<0+GET_CODE(_remedy_pkt)[4]<<std::endl;
            }
        }
        _blk_seq++;
        _tx_cnt = 0;
        _largest_pkt_size = 0;
    };
    return ret;
}


bool ncserver::open_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    // Socket initialization
    if(_is_opned == true)
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
    timeval tv = {0, 500};
    if(setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
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
    _redundant_pkts = 2;

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
    _is_opned = true;
    return true;
}

unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size)
{
    std::lock_guard<std::mutex> lock(_lock);
    unsigned short int ret = 0;
    if(_is_opned == false)
    {
        return 0;
    }
    if(pkt_size > MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE))
    {
        return 0; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
    }
    if(_largest_pkt_size < pkt_size)
    {
        _largest_pkt_size = pkt_size;
    }

    GET_BLK_SEQ(_buffer[_tx_cnt]) = _blk_seq;
    GET_BLK_SIZE(_buffer[_tx_cnt]) = _MAX_BLOCK_SIZE;
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
        _tx_cnt++;
        _timer->start(_TX_TIMEOUT, _CANCEL_CALLBACK, _TIMEOUT_CALLBACK);
    }
    else if(_tx_cnt < _MAX_BLOCK_SIZE-1)
    {
        _tx_cnt++;
    }
    else if(_tx_cnt == _MAX_BLOCK_SIZE-1)
    {
        _tx_cnt++;
        _timer->cancel();
    }
    return ret;
}

void ncserver::close_server()
{
    std::lock_guard<std::mutex> lock(_lock);
    // Socket initialization
    if(_is_opned == false)
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
    _is_opned = false;
}
