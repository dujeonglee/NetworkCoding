#include "ncclient.h"
#include "finite_field.h"
#include <string.h>
#include <iostream>

ncclient::ncclient(unsigned short int port, unsigned char block_size) : \
    _DATA_ADDR(_build_addr(INADDR_ANY, port)), _MAX_BLOCK_SIZE(block_size)
{
    _state = ncclient::CLOSE;
}

ncclient::~ncclient()
{
    close_client();
}

unsigned char ncclient::_code_size(unsigned char *pkt, bool original)
{
    if((GET_FLAGS(pkt) & FLAGS_END_OF_BLK) == 0)
    {
        return 0;
    }
    if(original)
    {
        for(unsigned char ret = 0 ; ret < _MAX_BLOCK_SIZE ; ret++)
        {
            if(GET_CODE(pkt)[ret] == 1)
            {
                return ret+1;
            }
        }
        return 0;
    }
    else
    {
        for(unsigned char ret = 0 ; ret < _MAX_BLOCK_SIZE ; ret++)
        {
            if(GET_CODE(pkt)[ret] == 0)
            {
                return ret;
            }
        }
        return _MAX_BLOCK_SIZE;
    }
}

bool ncclient::_inovative(unsigned char* pkt, int size)
{
    unsigned char* EMPTY_CODE = new unsigned char [_MAX_BLOCK_SIZE];
    memset(EMPTY_CODE, 0x0, _MAX_BLOCK_SIZE);
    if(_rank > 0)
    {
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
        {
            if(memcmp(EMPTY_CODE, GET_CODE(_buffer[i]), _MAX_BLOCK_SIZE) == 0)
            {
                continue;
            }
            unsigned char mul = FiniteField::instance()->inv(GET_CODE(pkt)[i]);
            for(unsigned int position = OUTERHEADER_SIZE ; \
                position < HEADER_SIZE(_MAX_BLOCK_SIZE)+size ; \
                position++)
            {
                pkt[position] = FiniteField::instance()->mul(pkt[position], mul);
                pkt[position] = pkt[position] ^ _buffer[i][position];
            }
        }
    }
    int inovative_code = 0;
    for(inovative_code = 0 ; inovative_code < _MAX_BLOCK_SIZE ; inovative_code++)
    {
        if(GET_CODE(pkt)[inovative_code] != 0)
        {
            break;
        }
    }
    if(inovative_code < _MAX_BLOCK_SIZE)
    {
        memcpy(_buffer[inovative_code], pkt, OUTERHEADER_SIZE);
        unsigned char mul = FiniteField::instance()->inv(GET_CODE(pkt)[inovative_code]);
        for(unsigned int position = OUTERHEADER_SIZE ; \
            position < HEADER_SIZE(_MAX_BLOCK_SIZE)+size ; \
            position++)
        {
            _buffer[inovative_code][position] = FiniteField::instance()->mul(pkt[position], mul);
        }
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
        {
            if(i  == inovative_code)
            {
                continue;
            }
            if(memcmp(EMPTY_CODE, GET_CODE(_buffer[i]), _MAX_BLOCK_SIZE) == 0)
            {
                continue;
            }
            unsigned char mul = GET_CODE(_buffer[i])[inovative_code];
            for(unsigned int position = OUTERHEADER_SIZE ; \
                position < HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE); \
                position++)
            {
                _buffer[i][position] = FiniteField::instance()->mul(_buffer[inovative_code][position], mul) ^ _buffer[i][position];
            }
        }
    }
    delete [] EMPTY_CODE;
    return inovative_code < _MAX_BLOCK_SIZE;
}

void ncclient::_receive_handler()
{
    while(_rx_thread_running)
    {
        sockaddr_in svr_addr;
        socklen_t svr_addr_length = sizeof(svr_addr);
        int ret = recvfrom(_socket, _rx_buffer, sizeof(_rx_buffer), 0, (sockaddr*)&svr_addr, &svr_addr_length);
        if(ret <= 0)
        {
            continue;
        }

        const unsigned short int blk_seq = GET_BLK_SEQ(_rx_buffer);
        if(blk_seq != _blk_seq)
        {
            if(_rank == 0 && blk_seq == (_blk_seq < 0xffff ? _blk_seq + 1 : 1 )) // New block is started
            {
                _blk_seq = blk_seq;
                _rank = 0;
                _next_pkt_index = 0;
                for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                {
                    memset(_buffer[i], 0x0, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                }
            }
            else
            {
                /*
                 * Receive packets not associated with the current block.
                 * Discard the data packet and send an ack packet with the current block sequence number.
                 * Server should adjust its block sequence number with the sequence in the ack packet and retransmit all packets in the current block again.
                 */
                if((GET_FLAGS(_rx_buffer) & FLAGS_END_OF_BLK))
                {
                    ret = sendto(_socket, (void*)&_blk_seq, sizeof(_blk_seq), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                    if(ret<=0)
                    {
                        std::cout<<"Could not send ack\n";
                    }
                    _rank = 0;
                    _next_pkt_index = 0;
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        memset(_buffer[i], 0x0, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                    }
                }
                continue;
            }
        }

        /*
         * Receive packets associated with the current block.
         */
        const unsigned char* code = GET_CODE(_rx_buffer);
         int index = -1;
        for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
        {
            if(code[i] == 1)
            {
                index = i;
            }
        }
        if(index > -1) // Original packet
        {
            memcpy(_buffer[index], _rx_buffer, ret);
            if(_rank == _next_pkt_index)
            {
                _lock.lock();
                if(_receive_callback != nullptr)
                {
                    _receive_callback(GET_PAYLOAD(_rx_buffer, _MAX_BLOCK_SIZE), ret - HEADER_SIZE(_MAX_BLOCK_SIZE));
                }
                _lock.unlock();
                unsigned char code_size = _code_size(_rx_buffer, true);
                if((GET_FLAGS(_rx_buffer) & FLAGS_END_OF_BLK) && _rank == code_size)
                {
                    ret = sendto(_socket, (void*)&blk_seq, sizeof(blk_seq), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                    if(ret<=0)
                    {
                        std::cout<<"Could not send ack\n";
                    }
                    _rank = 0;
                    _next_pkt_index = 0;
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        memset(_buffer[i], 0x0, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                    }
                    continue;
                }
                _next_pkt_index++;
            }
            _rank++;
        }
        else // Remedy packet
        {
            unsigned char code_size = _code_size(_rx_buffer, false);
            if(code_size == 0)
            {
                if(_rank == GET_BLK_SIZE(_rx_buffer))
                {
                    ret = sendto(_socket, (void*)&blk_seq, sizeof(blk_seq), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                    if(ret<=0)
                    {
                        std::cout<<"Could not send ack\n";
                    }
                    _rank = 0;
                    _next_pkt_index = 0;
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        memset(_buffer[i], 0x0, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                    }
                }
            }
            else
            {
                if(_inovative(_rx_buffer, ret) == true)
                {
                    _rank++;
                }
                if(_rank == code_size)
                {
                    std::cout <<0+blk_seq<<" OK Decode\n";
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        for(int j = 0 ; j < _MAX_BLOCK_SIZE ; j++)
                        {
                            std::cout<<0+GET_CODE(_buffer[i])[j]<<" ";
                        }
                        std::cout <<"\n";
                    }
                    ret = sendto(_socket, (void*)&blk_seq, sizeof(blk_seq), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                    if(ret<=0)
                    {
                        std::cout<<"Could not send ack\n";
                    }
                    _rank = 0;
                    _next_pkt_index = 0;
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        memset(_buffer[i], 0x0, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                    }
                }
            }
        }
    }
}


bool ncclient::open_client(std::function <void (unsigned char*, unsigned int length)> rx_handler){
    std::lock_guard<std::mutex> lock(_lock);

    if(_state == ncclient::OPEN)
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

    const timeval tv = {0, 500};
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

    if(bind(_socket, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == -1)
    {
        close(_socket);
        _socket = -1;
        return false;
    }

    try{
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
    }catch(std::exception &ex){
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    int buffer_index = 0;
    try{
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++){
            _buffer[buffer_index] = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
        }
    }catch(std::exception &ex){
        for(--buffer_index; buffer_index >-1 ; buffer_index--){
            delete [] _buffer[buffer_index];
        }
        delete [] _buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    _receive_callback = rx_handler;
    _rank = 0;
    _next_pkt_index = 0;
    _blk_seq = 0;
    _rx_thread_running = true;
    _rx_thread = std::thread([&](){this->_receive_handler();});
    _state = ncclient::OPEN;
    return true;
}

void ncclient::close_client()
{
    std::lock_guard<std::mutex> lock(_lock);

    if(_state == ncclient::CLOSE)
    {
        return;
    }
     _rx_thread_running = false;
     _rx_thread.join();

     _receive_callback = nullptr;

     for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
     {
         delete [] (_buffer[i]);
     }
     delete []_buffer;
     _buffer = nullptr;
     close(_socket);
     _socket = -1;
     _state = ncclient::CLOSE;
}
