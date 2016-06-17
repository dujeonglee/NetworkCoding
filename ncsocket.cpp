#include "ncsocket.h"

sockaddr_in addr(unsigned int ip, unsigned short int port)
{
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = ip;
    ret.sin_port = port;
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
    _nc_tx = new nctx(_socket);
    _nc_rx = new ncrx(_socket, rx);
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
            _nc_tx->_rx_handler(_rx_buffer, (unsigned int)ret, &sender_addr, sender_addr_length);
            _nc_rx->_rx_handler(_rx_buffer, (unsigned int)ret, &sender_addr, sender_addr_length);
        }
    });
    _state = ncsocket::STATE::INIT_SUCCESS;
}

ncsocket::~ncsocket()
{
    _is_rx_thread_running = false;
    _rx_thread.join();
    delete _nc_tx;
    delete _nc_rx;
    close(_socket);
}

bool ncsocket::open_session(unsigned int ip, unsigned short int port, BLOCK_SIZE block_size, unsigned int timeout, unsigned char redundancy)
{
    return _nc_tx->open_session(ip, port, block_size, timeout, redundancy);
}

bool ncsocket::connect_session(unsigned int client_ip, unsigned short int cport, unsigned char probes, unsigned int timeout)
{
    return _nc_tx->connect_session(client_ip, cport, probes, timeout);
}

void ncsocket::close_session(unsigned int ip, unsigned short int port)
{
    _nc_tx->close_session(ip, port);
}

int ncsocket::send(unsigned int ip, unsigned short int port, unsigned char* buff, unsigned int size, bool force_start_retransmission, unsigned int ack_timeout, tx_session_param* new_param)
{
    return _nc_tx->send(ip, port, buff, size, force_start_retransmission, ack_timeout, new_param);
}
