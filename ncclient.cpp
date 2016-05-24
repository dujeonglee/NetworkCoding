#include "ncclient.h"
#include "finite_field.h"
#include <string.h>
#include <iostream>

#define UNROLL_MATRIX_MULTIPLICATION_2(output, position, row_index, accumulator) \
accumulator = FiniteField::instance()->mul(_decoding_matrix[row_index].decode[0], _buffer[0].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[1], _buffer[1].buffer[position]);\
output[position] = accumulator

#define UNROLL_MATRIX_MULTIPLICATION_4(output, position, row_index, accumulator) \
accumulator = FiniteField::instance()->mul(_decoding_matrix[row_index].decode[0], _buffer[0].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[1], _buffer[1].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[2], _buffer[2].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[3], _buffer[3].buffer[position]);\
output[position] = accumulator

#define UNROLL_MATRIX_MULTIPLICATION_8(output, position, row_index, accumulator) \
accumulator = FiniteField::instance()->mul(_decoding_matrix[row_index].decode[0], _buffer[0].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[1], _buffer[1].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[2], _buffer[2].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[3], _buffer[3].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[4], _buffer[4].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[5], _buffer[5].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[6], _buffer[6].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[7], _buffer[7].buffer[position]);\
output[position] = accumulator

#define UNROLL_MATRIX_MULTIPLICATION_16(output, position, row_index, accumulator) \
accumulator = FiniteField::instance()->mul(_decoding_matrix[row_index].decode[0], _buffer[0].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[1], _buffer[1].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[2], _buffer[2].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[3], _buffer[3].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[4], _buffer[4].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[5], _buffer[5].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[6], _buffer[6].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[7], _buffer[7].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[8], _buffer[8].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[9], _buffer[9].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[10], _buffer[10].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[11], _buffer[11].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[12], _buffer[12].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[13], _buffer[13].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[14], _buffer[14].buffer[position]);\
accumulator ^= FiniteField::instance()->mul(_decoding_matrix[row_index].decode[15], _buffer[15].buffer[position]);\
output[position] = accumulator

unsigned int ori = 0;
unsigned int coding = 0;

ncclient::ncclient(unsigned short int port, BLOCK_SIZE block_size) : \
    _DATA_ADDR(_build_addr(INADDR_ANY, port)), _MAX_BLOCK_SIZE(block_size)
{
    _state = ncclient::CLOSE;
}

ncclient::~ncclient()
{
    printf("Coding %u Orig %u\n", coding, ori);
    close_client();
}

bool ncclient::_handle_original_packet(const unsigned char * const pkt, int size)
{
    // Copy original packet to the relavant position in _buffer.
    // For original pkt, GET_OUTER_BLK_SIZE(pkt) is _tx_cnt.
    const unsigned short int index = GET_OUTER_BLK_SIZE(pkt);
    _buffer[index].delivered = false;
    memcpy(_buffer[index].buffer, pkt, GET_OUTER_SIZE(pkt));
    _decoding_matrix[index].empty = false;
    memcpy(_decoding_matrix[index].encode, GET_INNER_CODE(pkt), _MAX_BLOCK_SIZE);
    memcpy(_decoding_matrix[index].decode, GET_INNER_CODE(pkt), _MAX_BLOCK_SIZE);

    bool send_ack = false;
    if(_rank == GET_OUTER_BLK_SIZE(pkt))
    {
        _lock.lock();
        if(_receive_callback != nullptr)
        {
            ori++;
            _receive_callback(GET_INNER_PAYLOAD(_buffer[index].buffer, _MAX_BLOCK_SIZE), GET_INNER_SIZE(_buffer[index].buffer));
            _buffer[index].delivered = true; // The packet is delivered.
        }
        _lock.unlock();

        // All original packets in the block are received without losses.
        // Send the ack.
        if(GET_OUTER_FLAGS(_buffer[index].buffer) & OuterHeader::FLAGS_END_OF_BLK)
        {
            send_ack = true;
        }
    }
    if(send_ack == false)
    {
        _rank++;
    }
    return send_ack;
}

int ncclient::_innovative(unsigned char* pkt)
{
    // 1. find an empty slot.
    int innovative_index = 0;
    for( ; innovative_index < _MAX_BLOCK_SIZE ; innovative_index++)
    {
        if(_decoding_matrix[innovative_index].empty == true)
        {
            _decoding_matrix[innovative_index].empty = false;
            memcpy(_decoding_matrix[innovative_index].encode, GET_INNER_CODE(pkt), _MAX_BLOCK_SIZE);
            memset(_decoding_matrix[innovative_index].decode, 0x0, _MAX_BLOCK_SIZE);
            _decoding_matrix[innovative_index].decode[innovative_index] = 1;
            break;
        }
    }
    if(innovative_index >= _MAX_BLOCK_SIZE)
    {
        return -1;
    }

    // 2. eliminate redundunt information of pkt
    for(unsigned char buffer_index = 0 ; buffer_index < _MAX_BLOCK_SIZE ; buffer_index++)
    {
        if(_decoding_matrix[buffer_index].empty == true)
        {
            continue;
        }
        if(_decoding_matrix[innovative_index].encode[buffer_index] == 0)
        {
            continue;
        }
        if(buffer_index == innovative_index)
        {
            continue;
        }
        const unsigned char mul = _decoding_matrix[innovative_index].encode[buffer_index];
        for(unsigned char code_index = 0 ; code_index < _MAX_BLOCK_SIZE ; code_index++)
        {
            _decoding_matrix[innovative_index].encode[code_index] = FiniteField::instance()->mul(_decoding_matrix[buffer_index].encode[code_index], mul) ^ _decoding_matrix[innovative_index].encode[code_index];
            _decoding_matrix[innovative_index].decode[code_index] = FiniteField::instance()->mul(_decoding_matrix[buffer_index].decode[code_index], mul) ^ _decoding_matrix[innovative_index].decode[code_index];
        }
    }

    // 3. check innovativity
    if(_decoding_matrix[innovative_index].encode[innovative_index] == 0)
    {
        _decoding_matrix[innovative_index].empty = true;
        memset(_decoding_matrix[innovative_index].encode, 0x0, _MAX_BLOCK_SIZE);
        memset(_decoding_matrix[innovative_index].decode, 0x0, _MAX_BLOCK_SIZE);
        return -1;
    }

    // 4. Make _decoding_matrix[innovative_index].encode[innovative_index] to be 1.
    if(_decoding_matrix[innovative_index].encode[innovative_index] != 1)
    {
        const unsigned char mul = FiniteField::instance()->inv(_decoding_matrix[innovative_index].encode[innovative_index]);
        for(unsigned char code_index = 0 ; code_index < _MAX_BLOCK_SIZE ; code_index++)
        {
            _decoding_matrix[innovative_index].encode[code_index] = FiniteField::instance()->mul(_decoding_matrix[innovative_index].encode[code_index], mul);
            _decoding_matrix[innovative_index].decode[code_index] = FiniteField::instance()->mul(_decoding_matrix[innovative_index].decode[code_index], mul);
        }
    }

    // 5. remove redundunt information from the buffer
    for(unsigned char buffer_index = 0 ; buffer_index < _MAX_BLOCK_SIZE ; buffer_index++)
    {
        if(_decoding_matrix[buffer_index].empty == true)
        {
            continue;
        }
        if(_decoding_matrix[buffer_index].encode[innovative_index] == 0)
        {
            continue;
        }
        if(buffer_index == innovative_index)
        {
            continue;
        }
        const unsigned char mul = _decoding_matrix[buffer_index].encode[innovative_index];
        for(unsigned char code_index = 0 ; code_index < _MAX_BLOCK_SIZE ; code_index++)
        {
            _decoding_matrix[buffer_index].encode[code_index] = FiniteField::instance()->mul( _decoding_matrix[innovative_index].encode[code_index], mul ) ^ \
                    _decoding_matrix[buffer_index].encode[code_index];
            _decoding_matrix[buffer_index].decode[code_index] = FiniteField::instance()->mul( _decoding_matrix[innovative_index].decode[code_index], mul ) ^ \
                    _decoding_matrix[buffer_index].decode[code_index];
        }
    }

    return innovative_index;
}

void ncclient::_decode(unsigned char* pkt, int size)
{
    int innovative_index = _innovative(pkt);
    if(innovative_index == -1)
    {
        return;
    }
    _buffer[innovative_index].delivered = false;
    memcpy(_buffer[innovative_index].buffer, pkt, GET_OUTER_SIZE(pkt));
    _rank++;
    _losses++;
}

bool ncclient::_handle_remedy_packet(unsigned char *pkt, int size)
{
    if(_rank == GET_OUTER_BLK_SIZE(pkt)+1)
    {
        return true;
    }
    _decode(pkt, size);
    if(_rank == GET_OUTER_BLK_SIZE(pkt)+1)
    {
        for(unsigned char i = 0 ; i < _rank ; i++)
        {
            if(_buffer[i].delivered == false)
            {
                if((GET_OUTER_FLAGS(_buffer[i].buffer) & OuterHeader::FLAGS_ORIGINAL) == 0)
                {
                    memcpy(_rx_buffer, _buffer[i].buffer, OUTER_HEADER_SIZE);
                    // decoding
                    for(unsigned int decoding_position = OUTER_HEADER_SIZE ; decoding_position < GET_OUTER_SIZE(_buffer[i].buffer) ; )
                    {
                        if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 1023)
                        {
                            _unroll_decode_1024(_rx_buffer, decoding_position, i);
                            decoding_position+=1024;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 511)
                        {
                            _unroll_decode_512(_rx_buffer, decoding_position, i);
                            decoding_position+=512;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 255)
                        {
                            _unroll_decode_256(_rx_buffer, decoding_position, i);
                            decoding_position+=256;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 127)
                        {
                            _unroll_decode_128(_rx_buffer, decoding_position, i);
                            decoding_position+=128;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 63)
                        {
                            _unroll_decode_64(_rx_buffer, decoding_position, i);
                            decoding_position+=64;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 31)
                        {
                            _unroll_decode_32(_rx_buffer, decoding_position, i);
                            decoding_position+=32;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 15)
                        {
                            _unroll_decode_16(_rx_buffer, decoding_position, i);
                            decoding_position+=16;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 7)
                        {
                            _unroll_decode_8(_rx_buffer, decoding_position, i);
                            decoding_position+=8;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 3)
                        {
                            _unroll_decode_4(_rx_buffer, decoding_position, i);
                            decoding_position+=4;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 1)
                        {
                            _unroll_decode_2(_rx_buffer, decoding_position, i);
                            decoding_position+=2;
                        }
                        else if(GET_OUTER_SIZE(_buffer[i].buffer) - decoding_position > 0)
                        {
                            unsigned char accumulator;
                            switch(_MAX_BLOCK_SIZE)
                            {
                            case 2:
                                UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, decoding_position, i, accumulator);
                                break;
                            case 4:
                                UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, decoding_position, i, accumulator);
                                break;
                            case 8:
                                UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, decoding_position, i, accumulator);
                                break;
                            case 16:
                                UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, decoding_position, i, accumulator);
                                break;
                            }
                            decoding_position+=1;
                        }
                    }
                    _lock.lock();
                    if(_receive_callback != nullptr)
                    {
                        coding++;
                        _receive_callback(GET_INNER_PAYLOAD(_rx_buffer, _MAX_BLOCK_SIZE), GET_INNER_SIZE(_rx_buffer));
                        _buffer[i].delivered = true; // The packet is delivered.
                    }
                    _lock.unlock();
                }
                else
                {
                    _lock.lock();
                    if(_receive_callback != nullptr)
                    {
                        ori++;
                        _receive_callback(GET_INNER_PAYLOAD(_buffer[i].buffer, _MAX_BLOCK_SIZE), GET_INNER_SIZE(_buffer[i].buffer));
                        _buffer[i].delivered = true; // The packet is delivered.
                    }
                    _lock.unlock();
                }
            }
        }

        return true;
    }
    return false;
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
        /*
         * Random Packet Loss
         */
        if(rand()%5 == 0)
        {
            continue;
        }

        const unsigned short int blk_seq = GET_OUTER_BLK_SEQ(_rx_buffer);
        /*
         * A change on a block sequence number indicates start of new block.
         * We need to flush rx buffers.
         */
        if(blk_seq - _blk_seq == 1  || _blk_seq - blk_seq == 0xffff)
        {
            _rank = 0;
            _blk_seq = blk_seq;
            _losses = 0;
            for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++)
            {
                _buffer[i].delivered = false;
                memset(_buffer[i].buffer, 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
                _decoding_matrix[i].empty = true;
                memset(_decoding_matrix[i].encode, 0x0, _MAX_BLOCK_SIZE);
                memset(_decoding_matrix[i].decode, 0x0, _MAX_BLOCK_SIZE);
                _decoding_matrix[i].decode[i] = 1;
            }
        }

        bool send_ack = false;
        if(GET_OUTER_FLAGS(_rx_buffer) & OuterHeader::FLAGS_ORIGINAL)
        {
            send_ack = _handle_original_packet(_rx_buffer, ret);
        }
        else
        {
            send_ack = _handle_remedy_packet(_rx_buffer, ret);
        }
        if(send_ack)
        {
            Ack ack_pkt;
            ack_pkt.blk_seq = _blk_seq;
            ack_pkt.losses = _losses;
            ret = sendto(_socket, (void*)&ack_pkt, sizeof(ack_pkt), 0, (sockaddr*)&svr_addr, sizeof(svr_addr));
            if(ret != sizeof(ack_pkt))
            {
                std::cout<<"Could not send ack\n";
            }
        }
    }
}


void ncclient::_unroll_decode_2(unsigned char *output, unsigned short position, unsigned char row_index)
{
    unsigned char accumulator;
    switch(_MAX_BLOCK_SIZE)
    {
    case 2:
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+1, row_index, accumulator);
        break;
    case 4:
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+1, row_index, accumulator);
        break;
    case 8:
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+1, row_index, accumulator);
        break;
    case 16:
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+1, row_index, accumulator);
        break;
    }
}

void ncclient::_unroll_decode_4(unsigned char *output, unsigned short position, unsigned char row_index)
{
    unsigned char accumulator;
    switch(_MAX_BLOCK_SIZE)
    {
    case 2:
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+3, row_index, accumulator);
        break;
    case 4:
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+3, row_index, accumulator);
        break;
    case 8:
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+3, row_index, accumulator);
        break;
    case 16:
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+3, row_index, accumulator);
        break;
    }
}

void ncclient::_unroll_decode_8(unsigned char *output, unsigned short position, unsigned char row_index)
{
    unsigned char accumulator;
    switch(_MAX_BLOCK_SIZE)
    {
    case 2:
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+7, row_index, accumulator);
        break;
    case 4:
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+7, row_index, accumulator);
        break;
    case 8:
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+7, row_index, accumulator);
        break;
    case 16:
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+7, row_index, accumulator);
        break;
    }
}

void ncclient::_unroll_decode_16(unsigned char *output, unsigned short position, unsigned char row_index)
{
    unsigned char accumulator;
    switch(_MAX_BLOCK_SIZE)
    {
    case 2:
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+7, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+8, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+9, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+10, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+11, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+12, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+13, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+14, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_2(_rx_buffer, position+15, row_index, accumulator);
        break;
    case 4:
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+7, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+8, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+9, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+10, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+11, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+12, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+13, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+14, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_4(_rx_buffer, position+15, row_index, accumulator);
        break;
    case 8:
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+7, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+8, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+9, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+10, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+11, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+12, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+13, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+14, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_8(_rx_buffer, position+15, row_index, accumulator);
        break;
    case 16:
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+1, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+2, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+3, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+4, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+5, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+6, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+7, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+8, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+9, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+10, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+11, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+12, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+13, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+14, row_index, accumulator);
        UNROLL_MATRIX_MULTIPLICATION_16(_rx_buffer, position+15, row_index, accumulator);
        break;
    }
}

void ncclient::_unroll_decode_32(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_16(output, position, row_index);
    _unroll_decode_16(output, position+16, row_index);
}

void ncclient::_unroll_decode_64(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_16(output, position, row_index);
    _unroll_decode_16(output, position+16, row_index);
    _unroll_decode_16(output, position+32, row_index);
    _unroll_decode_16(output, position+48, row_index);
}

void ncclient::_unroll_decode_128(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_16(output, position, row_index);
    _unroll_decode_16(output, position+16, row_index);
    _unroll_decode_16(output, position+32, row_index);
    _unroll_decode_16(output, position+48, row_index);
    _unroll_decode_16(output, position+64, row_index);
    _unroll_decode_16(output, position+80, row_index);
    _unroll_decode_16(output, position+96, row_index);
    _unroll_decode_16(output, position+112, row_index);

}

void ncclient::_unroll_decode_256(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_128(output, position, row_index);
    _unroll_decode_128(output, position+128, row_index);
}

void ncclient::_unroll_decode_512(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_128(output, position, row_index);
    _unroll_decode_128(output, position+128, row_index);
    _unroll_decode_128(output, position+256, row_index);
    _unroll_decode_128(output, position+384, row_index);
}

void ncclient::_unroll_decode_1024(unsigned char *output, unsigned short position, unsigned char row_index)
{
    _unroll_decode_512(output, position, row_index);
    _unroll_decode_512(output, position+512, row_index);
}


bool ncclient::open_client(std::function<void (unsigned char *, unsigned int)> rx_handler)
{
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
        _buffer = new PktBuffer[_MAX_BLOCK_SIZE];
    }catch(std::exception &ex){
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    int buffer_index = 0;
    try{
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++){
            _buffer[buffer_index].delivered = false;
            _buffer[buffer_index].buffer = new unsigned char[TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
            memset(_buffer[buffer_index].buffer, 0x0, TOTAL_HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE));
        }
    }catch(std::exception &ex){
        for(--buffer_index; buffer_index >= 0 ; buffer_index--){
            delete [] _buffer[buffer_index].buffer;
        }
        delete [] _buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    try{
        _decoding_matrix = new DecodingBuffer[_MAX_BLOCK_SIZE];
    }catch(std::exception &ex){
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
            delete [] _buffer[i].buffer;
        }
        delete [] _buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    buffer_index = 0;
    try{
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++){
            _decoding_matrix[buffer_index].encode = new unsigned char[_MAX_BLOCK_SIZE];
            memset(_decoding_matrix[buffer_index].encode, 0x0, _MAX_BLOCK_SIZE);
        }
    }catch(std::exception &ex){
        for(--buffer_index ; buffer_index >= 0 ; buffer_index--){
            delete [] _decoding_matrix[buffer_index].encode;
        }
        delete [] _decoding_matrix;
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
            delete [] _buffer[i].buffer;
        }
        delete [] _buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    buffer_index = 0;
    try{
        for(buffer_index = 0 ; buffer_index < (int)_MAX_BLOCK_SIZE ; buffer_index++){
            _decoding_matrix[buffer_index].empty = true;
            _decoding_matrix[buffer_index].decode = new unsigned char[_MAX_BLOCK_SIZE];
            memset(_decoding_matrix[buffer_index].decode, 0x0, _MAX_BLOCK_SIZE);
            _decoding_matrix[buffer_index].decode[buffer_index] = 1;
        }
    }catch(std::exception &ex){
        for(--buffer_index ; buffer_index >= 0 ; buffer_index--){
            delete [] _decoding_matrix[buffer_index].decode;
        }
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
            delete [] _decoding_matrix[i].encode;
        }
        delete [] _decoding_matrix;
        for(int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
            delete [] _buffer[i].buffer;
        }
        delete [] _buffer;
        _buffer = nullptr;
        close(_socket);
        _socket = -1;
        return false;
    }

    _receive_callback = rx_handler;
    _rank = 0;
    _blk_seq = 0;
    _losses = 0;
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
        delete [] (_buffer[i].buffer);
    }
    delete []_buffer;
    _buffer = nullptr;
    close(_socket);
    _socket = -1;
    _state = ncclient::CLOSE;
}
