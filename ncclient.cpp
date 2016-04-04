#include "ncclient.h"
#include "finite_field.h"
#include <iostream>

ncclient::ncclient(unsigned short int port, unsigned char block_size) : \
    _DATA_ADDR(_build_addr(INADDR_ANY, port)), _MAX_BLOCK_SIZE(block_size) {

}

ncclient::~ncclient(){
    close_client();
}

sockaddr_in ncclient::_build_addr(unsigned int ip, unsigned short int port){
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

bool ncclient::_is_original_pkt(unsigned char* pkt, unsigned int max_blk){
    const unsigned char* code = GET_CODE(pkt);
    unsigned int code_sum = 0;
    for(unsigned int i = 0 ; i < max_blk ; i++){
        code_sum += code[i];
        if(code_sum > 1){
            return false;
        }
    }
    return true;
}


bool ncclient::open_client(std::function <void (unsigned char*, unsigned int length)> rx_handler){
    std::lock_guard<std::mutex> lock(_lock);

    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1){
        return false;
    }
    int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        close(_socket);
        std::cout<<"Error 2\n";
        return false;
    }
    timeval tv = {1, 0};
    if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1){
        close(_socket);
        std::cout<<"Error 3\n";
        return false;
    }
    if(bind(_socket, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == -1){
        close(_socket);
        std::cout<<"Error 4\n";
        return false;
    }
    try{
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
        for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
            _buffer[i] = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
        }
    }catch(std::exception &ex){
        return false;
    }
    _rx_handler = rx_handler;
    _rx_thread_running = true;
    _rx_thread = std::thread([&](){
        _exit_rx_thread = false;
        while(_rx_thread_running){
            int ret;
            sockaddr_in svr_addr;
            socklen_t svr_addr_length = sizeof(svr_addr);
            ret = recvfrom(_socket, _rx_buffer, sizeof(_rx_buffer), 0, (sockaddr*)&svr_addr, &svr_addr_length);
            if(ret <= 0){
                continue;
            }
            unsigned char* code;
            code =GET_CODE(_rx_buffer);
            printf("RX Code:");
            for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
                printf(" %hhu", code[i]);
            }
            printf("\n");
            /*
            if(GET_BLK_SEQ(_rx_buffer) != _blk_seq){
                _rank = 0;
            }
            if(_is_original_pkt(_rx_buffer, _MAX_BLOCK_SIZE) == true){
                _rx_handler(GET_PAYLOAD(_rx_buffer, _MAX_BLOCK_SIZE), GET_SIZE(_rx_buffer));
                memcpy(_buffer[_rank], _rx_buffer, ret);
                _rank++;
                continue;
            }else{
                if(_rank < _MAX_BLOCK_SIZE){
                    unsigned char* code = GET_CODE(_rx_buffer);
                    for(unsigned int i = 0 ; i < _rank ; i++){
                        unsigned char mul = FiniteField::instance()->inv(code[i]);
                        for(unsigned int position = OUTERHEADER_SIZE ; position <  ret -  OUTERHEADER_SIZE ; position++){
                            _rx_buffer[position] = FiniteField::instance()->mul(_rx_buffer[position], mul) ^ _rx_buffer;
                        }
                    }
                    memcpy(_buffer)
                }else{
                    continue;
                }
            }
            */
        }
        _exit_rx_thread = true;
        std::cout<<"Rx thread is stopped\n";
    });
    _rx_thread.detach();
    return true;
}

void ncclient::close_client(){
    std::lock_guard<std::mutex> lock(_lock);

     if(_socket){
         _rx_thread_running = false;
         while(_exit_rx_thread == false);
         printf("_MAX_BLOCK_SIZE = %u\n", _MAX_BLOCK_SIZE);
         for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
             delete [] (_buffer[i]);
         }
         delete []_buffer;
         close(_socket);
         _socket = 0;
     }
}
