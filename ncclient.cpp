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

bool ncclient::_inovative(unsigned char* pkt, int size)
{
    unsigned char* EMPTY_CODE = new unsigned char [_MAX_BLOCK_SIZE];
    memset(EMPTY_CODE, 0x0, _MAX_BLOCK_SIZE);

    int inovative_index = -1;
    for(unsigned char i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
    {
        if(memcmp(EMPTY_CODE, GET_INNER_CODE(_buffer[i]), _MAX_BLOCK_SIZE) == 0)
        {
            if(inovative_index == -1)
            {
                inovative_index = (int)i;
            }
            continue;
        }
        {
            if(GET_INNER_CODE(pkt)[i] == 0)
            {
                continue;
            }
            const unsigned char mul = FiniteField::instance()->inv(GET_INNER_CODE(pkt)[i]);
            for(unsigned int position = OUTER_HEADER_SIZE ; position < (GET_OUTER_SIZE(pkt)<GET_OUTER_SIZE(_buffer[i])?GET_OUTER_SIZE(pkt):GET_OUTER_SIZE(_buffer[i])) ; position++)
            {
                pkt[position] = FiniteField::instance()->mul(pkt[position], mul) ^ _buffer[i][position];
            }
        }
    }
	/* Remedy packet is not innovative. We simply discard the packet. */
    if(inovative_index == -1 || GET_INNER_CODE(pkt)[inovative_index] == 0)
    {
		delete [] EMPTY_CODE;
        return false;
    }

    {
        const unsigned char mul = FiniteField::instance()->inv(GET_INNER_CODE(pkt)[inovative_index]);
        for(unsigned int position = OUTER_HEADER_SIZE ; position < GET_OUTER_SIZE(pkt) ; position++)
        {
            pkt[position] = FiniteField::instance()->mul(pkt[position], mul);
        }
    }

    memcpy(_buffer[inovative_index], pkt, GET_OUTER_SIZE(pkt));

    delete [] EMPTY_CODE;
    return inovative_index < _MAX_BLOCK_SIZE;
}

void ncclient::_decode()
{
    for(int i = _MAX_BLOCK_SIZE-1 ; i > -1 ; i--)
    {
        if(GET_INNER_CODE(_buffer[i])[i] != 1)
        {
            continue;
        }
        for(int j = i-1 ; j > -1 ; j--)
        {
            if(GET_INNER_CODE(_buffer[j])[i] == 0)
            {
                continue;
            }
            const unsigned char mul = GET_INNER_CODE(_buffer[j])[i];
            for(unsigned int position = OUTER_HEADER_SIZE ; 
			position < (GET_OUTER_SIZE(_buffer[i])<GET_OUTER_SIZE(_buffer[j])?GET_OUTER_SIZE(_buffer[i]):GET_OUTER_SIZE(_buffer[j])) ; position++)
            {
                _buffer[j][position] = _buffer[j][position] ^ FiniteField::instance()->mul(_buffer[i][position], mul);
            }
        }
    }
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
#ifdef RANDOM_PKT_LOSS
        /*
         * Random Packet Loss
         */
        if(rand()%2 == 0)
        {
            continue;
        }
#endif
        const unsigned short int blk_seq = GET_OUTER_BLK_SEQ(_rx_buffer);
        /*
         * A change on a block sequence number indicates start of new block.
         * We need to flush rx buffers.
         */
        if(_blk_seq != blk_seq)
        {
            _blk_seq = blk_seq;
            _rank = 0;
            _next_pkt_index = 0;
            for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
            {
                memset(_buffer[i], 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
            }
        }

        const int index = (GET_OUTER_FLAGS(_rx_buffer) & OuterHeader::FLAGS_ORIGINAL?GET_OUTER_BLK_SIZE(_rx_buffer):0) - 1;
        if(index != -1) // Original packet
        {
            memcpy(_buffer[index], _rx_buffer, ret);
            if(index == _next_pkt_index)
            {
                _lock.lock();
                if(_receive_callback != nullptr)
                {
                    _receive_callback(GET_INNER_PAYLOAD(_rx_buffer, _MAX_BLOCK_SIZE), GET_INNER_SIZE(_rx_buffer));
                    _next_pkt_index++;
                }
                _lock.unlock();
                if( (_rank == GET_OUTER_BLK_SIZE(_rx_buffer)-1) && (GET_OUTER_FLAGS(_rx_buffer) & OuterHeader::FLAGS_END_OF_BLK))
                {
                    Ack ack_pkt;
                    ack_pkt.blk_seq = _blk_seq;
                    ret = sendto(_socket, (void*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                    if(ret!=sizeof(ack_pkt))
                    {
                        std::cout<<"Could not send ack\n";
                    }
                    _rank = 0;
                    _next_pkt_index = 0;
                    for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                    {
                        memset(_buffer[i], 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                    }
                    continue;
                }
            }
            _rank++;
        }
        else // Remedy packet
        {
			bool local_deliver = false;
            if((local_deliver = (_inovative(_rx_buffer, ret) == true)))
            {
                _rank++;
            }
            if(_rank == GET_OUTER_BLK_SIZE(_rx_buffer))
            {
				if(local_deliver)
				{
					_decode();
					for(; _next_pkt_index < GET_OUTER_BLK_SIZE(_rx_buffer) ; )
					{
						_lock.lock();
						_receive_callback(GET_INNER_PAYLOAD(_buffer[_next_pkt_index], _MAX_BLOCK_SIZE),
										  GET_INNER_SIZE(_buffer[_next_pkt_index]));
						_next_pkt_index++;
						_lock.unlock();
					}
				}
                Ack ack_pkt;
                ack_pkt.blk_seq = _blk_seq;
                ret = sendto(_socket, (void*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
                if(ret!=sizeof(ack_pkt))
                {
                    std::cout<<"Could not send ack\n";
                }
                _rank = 0;
                _next_pkt_index = 0;
                for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
                {
                    memset(_buffer[i], 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
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
            _buffer[buffer_index] = new unsigned char[TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
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
