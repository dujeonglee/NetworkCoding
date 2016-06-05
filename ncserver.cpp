#include "ncserver.h"

sockaddr_in addr(unsigned int ip, unsigned short int port)
{
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

client_session_info::client_session_info(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size):\
    _DATA_ADDR(addr(client_ip, cport)), _MAX_BLOCK_SIZE(block_size) {
    _state = client_session_info::STATE::INIT_FAILURE;
    try{
        _buffer = new NetworkCodingPktBuffer [_MAX_BLOCK_SIZE];
    }catch(std::exception ex){
        _buffer = nullptr;
        return;
    }

    try{
        _rand_coef = new unsigned char [_MAX_BLOCK_SIZE];
    }catch(std::exception ex){
        delete [] _buffer;
        return;
    }
    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = 0;
    _loss_rate = 0.;
    _retransmission_in_progress = false;
    _state = client_session_info::STATE::INIT_SUCCESS;
}

client_session_info::~client_session_info()
{
    if(_state == client_session_info::STATE::INIT_FAILURE)
    {
        return;
    }
    delete [] _buffer;
    delete [] _rand_coef;
}

/*
 * Send remedy packets.
 */
bool ncserver::_send_remedy_pkt(client_session_info * const info)
{
    if(info->_retransmission_in_progress == false)
    {
        return false;
    }
    if(info->_tx_cnt == 0)
    {
        return (int)GET_OUTER_SIZE(info->_buffer[0].buffer) == sendto(_socket, info->_buffer[0].buffer, GET_OUTER_SIZE(info->_buffer[0].buffer), 0, (sockaddr*)&info->_DATA_ADDR, sizeof(info->_DATA_ADDR));
    }
    memset(info->_remedy_pkt.buffer, 0x0, TOTAL_HEADER_SIZE(info->_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(info->_MAX_BLOCK_SIZE));
    // Generate random cofficient.
    for(unsigned int code_id = 0 ; code_id < info->_MAX_BLOCK_SIZE ; code_id++)
    {
        // I want all packets are encoded for remedy pkts, i.e. coefficients are greater than 0.
        info->_rand_coef[code_id] = code_id <= info->_tx_cnt?1+rand()%255:0;
    }

    // Fill-in header information
    GET_OUTER_SIZE(info->_remedy_pkt.buffer) = TOTAL_HEADER_SIZE(info->_MAX_BLOCK_SIZE)+info->_largest_pkt_size;
    GET_OUTER_BLK_SEQ(info->_remedy_pkt.buffer) = info->_blk_seq;
    GET_OUTER_BLK_SIZE(info->_remedy_pkt.buffer) = info->_tx_cnt;
    GET_OUTER_MAX_BLK_SIZE(info->_remedy_pkt.buffer) = info->_MAX_BLOCK_SIZE;
    GET_OUTER_FLAGS(info->_remedy_pkt.buffer) = OuterHeader::FLAGS_END_OF_BLK;
    // Encode packets.
    for(unsigned int packet_id = 0 ; packet_id <= info->_tx_cnt ; packet_id++)
    {
        for(unsigned int coding_position = OUTER_HEADER_SIZE ;
            coding_position < GET_OUTER_SIZE(info->_buffer[packet_id].buffer)/*NOTE: 0^x = 0*/ ;
            coding_position++)
        {
            info->_remedy_pkt.buffer[coding_position] ^= FiniteField::instance()->mul(info->_buffer[packet_id].buffer[coding_position], info->_rand_coef[packet_id]);
        }
    }
    if(info->_retransmission_in_progress == false)
    {
        return false;
    }
    const bool ret = (int)GET_OUTER_SIZE(info->_remedy_pkt.buffer) == sendto(_socket, info->_remedy_pkt.buffer, GET_OUTER_SIZE(info->_remedy_pkt.buffer), 0, (sockaddr*)&info->_DATA_ADDR, sizeof(info->_DATA_ADDR));
    return ret;
}

ncserver::ncserver(unsigned short int svrport, unsigned int timeout):\
    _CTRL_ADDR(addr(INADDR_ANY, svrport)), _TIMEOUT(timeout)
{
    _state = ncserver::STATE::INIT_FAILURE;
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
    const timeval tv = {_TIMEOUT/1000, _TIMEOUT%1000};
    if(setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    if(setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    if(bind(_socket, (sockaddr*)&_CTRL_ADDR, sizeof(_CTRL_ADDR)) == -1)
    {
        close(_socket);
        _socket = -1;
        return;
    }
    _ack_reception_thread_running = true;
    _ack_reception_thread = std::thread([&](){
        while(_ack_reception_thread_running)
        {
            Ack ack_pkt = {0,};
            struct sockaddr_in client_addr = {0,};
            unsigned int client_addr_size = sizeof(client_addr);
            int ret = recvfrom(_socket, (void*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&client_addr, &client_addr_size);
            if(ret == sizeof(ack_pkt))
            {
                const ip_port_key key = {ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)};
                client_session_info** const session = _tx_session_info.find(key);
                if(session != nullptr)
                {
                    if(ack_pkt.blk_seq == (*session)->_blk_seq)
                    {
                        (*session)->_retransmission_in_progress = false;
                        (*session)->_loss_rate = ((*session)->_loss_rate + (float)ack_pkt.losses)/2.;
                    }
                    else
                    {
                        // Packets of new block sequence may be lost. We can ignore this packet.
                    }
                }
                else
                {
                    // To Do: Deal with zombie client
                }
            }
        }
    });
    _ack_reception_thread.detach();
    _state = ncserver::STATE::INIT_SUCCESS;
}

/*
 * Destructor: Release resources by "close_server".
 */
ncserver::~ncserver()
{
    if(_state == ncserver::STATE::INIT_FAILURE)
    {
        return;
    }
    _tx_session_info.perform_for_all_data([](client_session_info* session){
        delete session;
        session = nullptr;
    });
    close(_socket);
    _socket = -1;
}

bool ncserver::open_session(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size)
{
    if(_state == ncserver::STATE::INIT_FAILURE)
    {
        return false;
    }
    client_session_info* new_tx_session_info = nullptr;
    const ip_port_key key = {client_ip, cport};
    if(_tx_session_info.find(key) == false)
    {
        try
        {
            new_tx_session_info = new client_session_info(client_ip, cport, block_size);
        }
        catch(std::exception ex)
        {
            return false;
        }
        if(new_tx_session_info->_state == client_session_info::STATE::INIT_FAILURE)
        {
            delete new_tx_session_info;
            return false;
        }
        if(_tx_session_info.insert(key, new_tx_session_info) == false)
        {
            delete new_tx_session_info;
            return false;
        }
    }
    return true;
}

void ncserver::close_session(unsigned int client_ip, unsigned short int cport)
{
    if(_state == ncserver::STATE::INIT_FAILURE)
    {
        return;
    }
    const ip_port_key key = {client_ip, cport};
    client_session_info** session = nullptr;
    if((session = _tx_session_info.find(key)) != nullptr)
    {
        delete (*session);
        _tx_session_info.remove(key);
    }
}

/*
 * Send data to the client.
 * pkt: Pointer for data.
 * pkt_size: Size of data.
 */
unsigned short int ncserver::send(unsigned int client_ip, unsigned short int cport, unsigned char* pkt, unsigned short int pkt_size, const bool complete_block)
{
    if(_state == ncserver::STATE::INIT_FAILURE)
    {
        printf("asdfasdf\n");
        return 0;
    }
    const ip_port_key key = {client_ip, cport};
    client_session_info** const session = _tx_session_info.find(key);
    if(session == nullptr)
    {
        return 0;
    }
    if((*session)->_state == client_session_info::STATE::INIT_FAILURE)
    {
        return 0;
    }
    if(pkt_size > MAX_PAYLOAD_SIZE((*session)->_MAX_BLOCK_SIZE))
    {
        // A single packet size is limitted to MAX_PAYLOAD_SIZE for the simplicity.
        // Support of large data packet is my future work.
        return 0;
    }
    if((*session)->_largest_pkt_size < pkt_size)
    {
        // _largest_pkt_size maintains the largest packet size of data packets with the same block ID.
        // _largest_pkt_size is used to efficiently encode data packets.
        // Note: Packets with the same block ID will be encoded together to repair random packet losses.
        (*session)->_largest_pkt_size = pkt_size;
    }
    // Fill outer header
    (*session)->_lock.lock();
    const unsigned char* pkt_buffer = (*session)->_buffer[(*session)->_tx_cnt].buffer;
    const bool invoke_retransmission = ((*session)->_tx_cnt == (*session)->_MAX_BLOCK_SIZE - 1) || (complete_block == true);

    GET_OUTER_BLK_SEQ(pkt_buffer) = (*session)->_blk_seq;
    GET_OUTER_SIZE(pkt_buffer) = TOTAL_HEADER_SIZE((*session)->_MAX_BLOCK_SIZE)+pkt_size;
    GET_OUTER_BLK_SIZE(pkt_buffer) = (*session)->_tx_cnt;
    GET_OUTER_MAX_BLK_SIZE(pkt_buffer) = (*session)->_MAX_BLOCK_SIZE;
    GET_OUTER_FLAGS(pkt_buffer) = OuterHeader::FLAGS_ORIGINAL;
    GET_OUTER_FLAGS(pkt_buffer)  = (invoke_retransmission?GET_OUTER_FLAGS(pkt_buffer) | OuterHeader::FLAGS_END_OF_BLK:GET_OUTER_FLAGS(pkt_buffer));
    const float loss_rate = (invoke_retransmission?(unsigned char)(*session)->_loss_rate:0);
    (*session)->_retransmission_in_progress = (invoke_retransmission?true:false);

    // Fill inner header
    GET_INNER_SIZE(pkt_buffer) = pkt_size;
    GET_INNER_LAST_INDICATOR(pkt_buffer) = 1; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
    memset(GET_INNER_CODE(pkt_buffer), 0x0, (*session)->_MAX_BLOCK_SIZE);
    GET_INNER_CODE(pkt_buffer)[(*session)->_tx_cnt] = 1; // Array index is 0 base.

    // Copy data into payload
    memcpy(GET_INNER_PAYLOAD(pkt_buffer, (*session)->_MAX_BLOCK_SIZE), pkt, pkt_size);

    // Send data packet
    //PRINT_OUTERHEADER(pkt_buffer);
    //PRINT_INNERHEADER(_MAX_BLOCK_SIZE, pkt_buffer);
    sendto(_socket, pkt_buffer, GET_OUTER_SIZE(pkt_buffer), 0, (sockaddr*)&(*session)->_DATA_ADDR, sizeof((*session)->_DATA_ADDR));
    if(invoke_retransmission)
    {
        for(unsigned char i = 0 ; i < loss_rate+1 ; i++)
        {
            _send_remedy_pkt((*session));
        }
        while((*session)->_retransmission_in_progress)
        {
            _send_remedy_pkt((*session));
            std::this_thread::sleep_for (std::chrono::milliseconds(0));
        }
        unsigned short next_blk_seq = 0;
        do
        {
            next_blk_seq = rand();
        }while(next_blk_seq == (*session)->_blk_seq);
        (*session)->_blk_seq = next_blk_seq;
        (*session)->_largest_pkt_size = 0;
        (*session)->_tx_cnt = 0;
    }
    else
    {
        (*session)->_tx_cnt++;
    }
    (*session)->_lock.unlock();
    return pkt_size;
}
