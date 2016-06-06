#include "ncsocket.h"

sockaddr_in addr(unsigned int ip, unsigned short int port)
{
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

ncsocket::ncsocket(unsigned short int port, unsigned int tx_timeout_milli, unsigned int rx_timeout_milli, std::function <void (unsigned char* buffer, unsigned int length, sockaddr_in addr)> rx)
{
    _state = ncsocket::STATE::INIT_FAILURE;
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1)
    {
        return;
    }
    const int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    const timeval tx_to = {tx_timeout_milli/1000, tx_timeout_milli%1000};
    if(setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tx_to, sizeof(tx_to)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    const timeval rx_to = {rx_timeout_milli/1000, rx_timeout_milli%1000};
    if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &rx_to, sizeof(rx_to)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    const sockaddr_in bind_addr = addr(INADDR_ANY, port);
    if(bind(_socket, (sockaddr*)&bind_addr, sizeof(bind_addr)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    _ncserver = new ncserver(_socket);
    _ncclient = new ncclient(_socket, rx);
    _is_rx_thread_running = true;
    _rx_thread =  std::thread([&](){
        while(_is_rx_thread_running)
        {
            sockaddr_in sender_addr = {0,};
            unsigned int sender_addr_length = sizeof(sockaddr_in);
            const int ret = recvfrom(_socket, _rx_buffer, ETHERNET_MTU, 0, (sockaddr*)&sender_addr, &sender_addr_length);
            if(ret <= 0)
            {
                continue;
            }
            _ncserver->_rx_handler(_rx_buffer, (unsigned int)ret, &sender_addr, sender_addr_length);
            _ncclient->_rx_handler(_rx_buffer, (unsigned int)ret, &sender_addr, sender_addr_length);
        }
    });
    _state = ncsocket::STATE::INIT_SUCCESS;
}

ncsocket::~ncsocket()
{
    _is_rx_thread_running = false;
    _rx_thread.join();
    delete _ncserver;
    delete _ncclient;
    close(_socket);
}

bool ncsocket::open_session(unsigned int ip, unsigned short int port, BLOCK_SIZE block_size, unsigned int timeout)
{
    return _ncserver->open_session(ip, port, block_size, timeout);
}

void ncsocket::close_session(unsigned int ip, unsigned short int port)
{
    _ncserver->close_session(ip, port);
}

int ncsocket::send(unsigned int ip, unsigned short int port, unsigned char* buff, unsigned int size, bool force_start_retransmission)
{
    return _ncserver->send(ip, port, buff, size, force_start_retransmission);
}
