#include "nctx.h"
#include <time.h>

extern sockaddr_in addr(unsigned int ip, unsigned short int port);

nctx::nctx(int socket): _SOCKET(socket){}
nctx::~nctx()
{
    _tx_session_info.perform_for_all_data([](tx_session_info* session){
        delete session;
        session = nullptr;
    });
}

tx_session_info::tx_session_info(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size, unsigned int retransmission_interval, unsigned char redundancy):\
    _DATA_ADDR(addr(client_ip, cport)), _max_block_size(block_size), _retransmission_interval(retransmission_interval) {
    _state = tx_session_info::STATE::INIT_FAILURE;
    try{
        _buffer = new NetworkCodingPktBuffer [_max_block_size];
        for(unsigned char i = 0 ; i < _max_block_size ; i++)
        {
            memset(_buffer[i].buffer, 0x0, sizeof(NetworkCodingPktBuffer));
        }
    }catch(std::exception ex){
        _buffer = nullptr;
        return;
    }

    try{
        _rand_coef = new unsigned char [_max_block_size];
        memset(_rand_coef, 0x0, _max_block_size);
    }catch(std::exception ex){
        delete [] _buffer;
        return;
    }
    _tx_cnt = 0;
    _largest_pkt_size = 0;
    _blk_seq = 0;
    _loss_rate = 0.;
    _redundancy = redundancy;
    _retransmission_in_progress = false;
    _state = tx_session_info::STATE::INIT_SUCCESS;
}

tx_session_info::~tx_session_info()
{
    if(_state == tx_session_info::STATE::INIT_FAILURE)
    {
        return;
    }
    _retransmission_in_progress = false;
    delete [] _buffer;
    delete [] _rand_coef;
}

/*
 * Send remedy packets.
 */
bool nctx::_send_remedy_pkt(tx_session_info * const info)
{
    if(info->_retransmission_in_progress == false)
    {
        return false;
    }
    if(info->_tx_cnt == 0)
    {
        return (int)GET_OUTER_SIZE(info->_buffer[0].buffer) == sendto(_SOCKET, info->_buffer[0].buffer, GET_OUTER_SIZE(info->_buffer[0].buffer), 0, (sockaddr*)&info->_DATA_ADDR, sizeof(info->_DATA_ADDR));
    }
    memset(info->_remedy_pkt.buffer, 0x0, TOTAL_HEADER_SIZE(info->_max_block_size)+MAX_PAYLOAD_SIZE(info->_max_block_size));
    // Generate random cofficient.
    for(unsigned int code_id = 0 ; code_id < info->_max_block_size ; code_id++)
    {
        // I want all packets are encoded for remedy pkts, i.e. coefficients are greater than 0.
        info->_rand_coef[code_id] = code_id <= info->_tx_cnt?1+rand()%255:0;
    }

    // Fill-in header information
    GET_OUTER_TYPE(info->_remedy_pkt.buffer) = NC_PKT_TYPE::DATA_TYPE;
    GET_OUTER_SIZE(info->_remedy_pkt.buffer) = TOTAL_HEADER_SIZE(info->_max_block_size)+info->_largest_pkt_size;
    GET_OUTER_BLK_SEQ(info->_remedy_pkt.buffer) = info->_blk_seq;
    GET_OUTER_BLK_SIZE(info->_remedy_pkt.buffer) = info->_tx_cnt;
    GET_OUTER_MAX_BLK_SIZE(info->_remedy_pkt.buffer) = info->_max_block_size;
    GET_OUTER_FLAGS(info->_remedy_pkt.buffer) = OuterHeader::FLAGS_END_OF_BLK;
    if(info->_retransmission_in_progress == false)
    {
        return false;
    }
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
    const bool ret = (int)GET_OUTER_SIZE(info->_remedy_pkt.buffer) == sendto(_SOCKET, info->_remedy_pkt.buffer, GET_OUTER_SIZE(info->_remedy_pkt.buffer), 0, (sockaddr*)&info->_DATA_ADDR, sizeof(info->_DATA_ADDR));
    return ret;
}

bool nctx::open_session(unsigned int client_ip, unsigned short int cport, BLOCK_SIZE block_size, unsigned int retransmission_interval, unsigned char redundancy)
{
    tx_session_info* new_tx_session_info = nullptr;
    const ip_port_key key = {client_ip, cport};
    if(_tx_session_info.find(key) == nullptr)
    {
        try
        {
            new_tx_session_info = new tx_session_info(client_ip, cport, block_size, retransmission_interval, redundancy);
        }
        catch(std::exception ex)
        {
            return false;
        }
        if(new_tx_session_info->_state == tx_session_info::STATE::INIT_FAILURE)
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

bool nctx::connect_session(unsigned int client_ip, unsigned short int cport, unsigned char probes, unsigned int to)
{
    const ip_port_key key = {client_ip, cport};
    const clock_t timeout = clock() + to*(CLOCKS_PER_SEC/1000);

    tx_session_info** const info = _tx_session_info.find(key);
    if(info == nullptr)
    {
        return false;
    }
    (*info)->_is_connected = false;
    for(unsigned char req = 0 ; req < probes ; req++)
    {
        Connect connect;
        connect.type = NC_PKT_TYPE::REQ_CONNECT_TYPE;
        if(sizeof(Connect) != sendto(_SOCKET, &connect, sizeof(Connect), 0, (sockaddr*)&(*info)->_DATA_ADDR, sizeof((*info)->_DATA_ADDR)))
        {
            std::cout<<"Could not send connection request\n";
        }
    }
    while((*info)->_is_connected == false && clock() < timeout);
    return (*info)->_is_connected;
}

void nctx::close_session(unsigned int client_ip, unsigned short int cport)
{
    const ip_port_key key = {client_ip, cport};
    tx_session_info** session = nullptr;
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
unsigned short int nctx::send(unsigned int client_ip, unsigned short int cport, unsigned char* pkt, unsigned short int pkt_size, const bool complete_block, const unsigned int ack_timeout, tx_session_param* new_param)
{
    const ip_port_key key = {client_ip, cport};
    tx_session_info** const session = _tx_session_info.find(key);
    if(session == nullptr)
    {
        return 0;
    }
    (*session)->_lock.lock();
    if((*session)->_state == tx_session_info::STATE::INIT_FAILURE)
    {
        (*session)->_lock.unlock();
        return 0;
    }
    if(pkt_size > MAX_PAYLOAD_SIZE((*session)->_max_block_size))
    {
        // A single packet size is limitted to MAX_PAYLOAD_SIZE for the simplicity.
        // Support of large data packet is my future work.
        (*session)->_lock.unlock();
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
    const unsigned char* pkt_buffer = (*session)->_buffer[(*session)->_tx_cnt].buffer;
    const bool invoke_retransmission = \
            ((*session)->_tx_cnt == (*session)->_max_block_size - 1) || \
            (complete_block == true) || \
            (new_param!=nullptr);

    GET_OUTER_TYPE(pkt_buffer) = NC_PKT_TYPE::DATA_TYPE;
    GET_OUTER_SIZE(pkt_buffer) = TOTAL_HEADER_SIZE((*session)->_max_block_size)+pkt_size;
    GET_OUTER_BLK_SEQ(pkt_buffer) = (*session)->_blk_seq;
    GET_OUTER_BLK_SIZE(pkt_buffer) = (*session)->_tx_cnt;
    GET_OUTER_MAX_BLK_SIZE(pkt_buffer) = (*session)->_max_block_size;
    GET_OUTER_FLAGS(pkt_buffer) = OuterHeader::FLAGS_ORIGINAL;
    GET_OUTER_FLAGS(pkt_buffer)  = (invoke_retransmission?GET_OUTER_FLAGS(pkt_buffer) | OuterHeader::FLAGS_END_OF_BLK:GET_OUTER_FLAGS(pkt_buffer));
    GET_INNER_SIZE(pkt_buffer) = pkt_size;
    GET_INNER_LAST_INDICATOR(pkt_buffer) = 1; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
    memset(GET_INNER_CODE(pkt_buffer), 0x0, (*session)->_max_block_size);
    GET_INNER_CODE(pkt_buffer)[(*session)->_tx_cnt] = 1; // Array index is 0 base.
    memcpy(GET_INNER_PAYLOAD(pkt_buffer, (*session)->_max_block_size), pkt, pkt_size);

    (*session)->_retransmission_in_progress = invoke_retransmission;
    const unsigned char loss_rate = ((*session)->_redundancy.load()==0xff?(invoke_retransmission?(*session)->_loss_rate.load()+1:(unsigned char)0)
                                                         :
                                                         (*session)->_redundancy.load());

    // Send data packet
    sendto(_SOCKET, pkt_buffer, GET_OUTER_SIZE(pkt_buffer), 0, (sockaddr*)&(*session)->_DATA_ADDR, sizeof((*session)->_DATA_ADDR));

    if(invoke_retransmission)
    {
        clock_t sent_time = 0;
        clock_t current_time = 0;
        for(unsigned char i = 0 ; i < loss_rate ; i++)
        {
            _send_remedy_pkt((*session));
        }
        const clock_t retransmission_timeout = clock() + (CLOCKS_PER_SEC/1000)*ack_timeout;
        /**
         * _redundancy: =0xff: Reliable transmission
         *                         <0xff: Best effort transmission.
         */
        while((*session)->_retransmission_in_progress && (*session)->_redundancy == 0xff && clock() < retransmission_timeout)
        {
            current_time = clock();
            if((current_time - sent_time)/(CLOCKS_PER_SEC/1000) > (*session)->_retransmission_interval)
            {
                _send_remedy_pkt((*session));
                sent_time = current_time;
            }
        }
        if((*session)->_redundancy == 0xff && (*session)->_retransmission_in_progress)
        {
            (*session)->_lock.unlock();
            return 0;
        }
        unsigned short next_blk_seq = 0;
        do
        {
            next_blk_seq = rand();
        }while(next_blk_seq == (*session)->_blk_seq);
        (*session)->_blk_seq = next_blk_seq;
        (*session)->_largest_pkt_size = 0;
        (*session)->_tx_cnt = 0;

        if(new_param)
        {
            if(new_param->block_size != (*session)->_max_block_size)
            {
                NetworkCodingPktBuffer* new_buffer = nullptr;
                unsigned char* new_rand_coef = nullptr;

                try{
                    new_buffer = new NetworkCodingPktBuffer [new_param->block_size];
                }
                catch(std::exception ex)
                {
                    // Packet is successfully delivered regardless of the result of updating nc parameters
                    return pkt_size;
                }

                try{
                    new_rand_coef = new unsigned char [new_param->block_size];
                }
                catch(std::exception ex)
                {
                    delete [] new_buffer;
                    // Packet is successfully delivered regardless of the result of updating nc parameters
                    return pkt_size;
                }
                delete [] ((*session)->_buffer);
                delete [] ((*session)->_rand_coef);
                (*session)->_buffer = new_buffer;
                (*session)->_rand_coef = new_rand_coef;
            }
            if(new_param->retransmission_interval != (*session)->_retransmission_interval)
            {
                (*session)->_retransmission_interval = new_param->retransmission_interval;
            }
            if(new_param->redundancy != (*session)->_redundancy)
            {
                (*session)->_redundancy = new_param->redundancy;
            }
            // Parameter is changed.
        }
    }
    else
    {
        (*session)->_tx_cnt++;
    }
    (*session)->_lock.unlock();
    return pkt_size;
}

void nctx::_rx_handler(unsigned char* buffer, unsigned int size, sockaddr_in* sender_addr, unsigned int sender_addr_len)
{
    if(buffer[0] == NC_PKT_TYPE::ACK_TYPE)
    {
        Ack* const ack = (Ack*)buffer;
        if(size != sizeof(Ack))
        {
            return;
        }
        const ip_port_key key = {sender_addr->sin_addr.s_addr, sender_addr->sin_port};
        tx_session_info** const session = _tx_session_info.find(key);
        if(session != nullptr)
        {
            if(ack->blk_seq == (*session)->_blk_seq)
            {
                (*session)->_retransmission_in_progress = false;
                (*session)->_loss_rate = ((*session)->_loss_rate + ack->losses)/2;
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
    else if(buffer[0] == NC_PKT_TYPE::REP_CONNECT_TYPE)
    {
        const ip_port_key key = {sender_addr->sin_addr.s_addr, sender_addr->sin_port};
        tx_session_info** const session = _tx_session_info.find(key);
        if(session)
        {
            (*session)->_is_connected = true;
        }
    }
}
