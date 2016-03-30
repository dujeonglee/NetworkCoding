#include "ncserver.h"
#include <exception>
#include <string.h>

ncserver::ncserver(unsigned int client_ip, unsigned short int port, \
                               unsigned char max_block_size, unsigned int timeout):\
                                    _CTRL_ADDR(build_addr(INADDR_ANY, port)), _DATA_ADDR(build_addr(client_ip, port)), \
                                    _MAX_BLOCK_SIZE(max_block_size), _TX_TIMEOUT(timeout){
    _socket = 0;
    _tx_cnt = 0;
    _blk_seq = 0;
    _pkt_seq = 0;
    _redundant_pkts = 0;
}

ncserver::~ncserver(){
    close_server();
}

sockaddr_in ncserver::build_addr(unsigned int ip, unsigned short int port){
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

void ncserver::_flush_pkts(){

}


bool ncserver::open_server(){
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1){
        return false;
    }
    int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        close(_socket);
        return false;
    }
    if(bind(_socket, (sockaddr*)&_CTRL_ADDR, sizeof(_CTRL_ADDR)) == -1){
        close(_socket);
        return false;
    }
    try{
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
    }catch(std::exception &ex){
        return false;
    }
    return true;
}

unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size){
    if(_tx_cnt == 0){
        _timer.start(_TX_TIMEOUT, this->_flush_pkts, this->_flush_pkts);

        _tx_cnt++;
    }else if(_tx_cnt < _MAX_BLOCK_SIZE-1){
        _tx_cnt++;
    }else{
        _timer.cancel();
    }
    return pkt_size;
}

void ncserver::close_server(){
    for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
        delete [] _buffer[i];
    }
    delete []_buffer;

    if(_socket){
        close(_socket);
    }
}
