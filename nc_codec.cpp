#include "nc_codec.h"
#include "finite_field.h"
#include <omp.h>

SINGLETON_PATTERN_INITIALIZATION_CPP(NetworkCodingCODEC){}

Decoder::Decoder(unsigned char coef_size){
    _coefficient_size = coef_size;
    _buffer.resize(_coefficient_size, NULL);
    _rank = 0;
}

Decoder::~Decoder(){
    _buffer.clear();
}

bool Decoder::push(unsigned char* p, const unsigned int pkt_size){
    const NetworkCodibgPacket* new_pkt = (NetworkCodibgPacket*)p;
    unsigned int coef_1 = 0;
    for(coef_1 = 0 ; coef_1 < _buffer.size() ; coef_1++ ){
        // 1. For each buffered packet, we eliminate redundant information of p from the packet.
        if(_buffer[coef_1] == NULL){
            continue;
        }
        const NetworkCodibgPacket* buffered_pkt = (NetworkCodibgPacket*)_buffer[coef_1];
        const unsigned char muliplier = FiniteField::instance()->inv((&new_pkt->payload_hdr.coefficients)[coef_1]);
        for(unsigned int code_pos = 0 ; code_pos < pkt_size-sizeof(NetworkCodingHeader) ; code_pos++){
            ((unsigned char*)&(new_pkt->payload_hdr))[code_pos] = FiniteField::instance()->mul(((unsigned char*)&(new_pkt->payload_hdr))[code_pos], muliplier);
            ((unsigned char*)&(new_pkt->payload_hdr))[code_pos] = FiniteField::instance()->add(((unsigned char*)&(new_pkt->payload_hdr))[code_pos], ((unsigned char*)&(buffered_pkt->payload_hdr))[code_pos]);
        }
    }
    // 2.Check if new packet has new information after the eliminations.
    unsigned char new_coef = 0;
    while(((unsigned char*)&(new_pkt->payload_hdr))[new_coef] == 0 && new_coef < _coefficient_size && _buffer[new_coef] != NULL){
        new_coef++;
    }
    // 3. if there is new information
    if(new_coef < _coefficient_size){
        _rank++;
        const unsigned char muliplier = FiniteField::instance()->inv((&new_pkt->payload_hdr.coefficients)[new_coef]);
        for(unsigned int code_pos = 0 ; code_pos < pkt_size-sizeof(NetworkCodingHeader) ; code_pos++){
            ((unsigned char*)&(new_pkt->payload_hdr))[code_pos] = FiniteField::instance()->mul(((unsigned char*)&(new_pkt->payload_hdr))[code_pos], muliplier);
        }
        _buffer[new_coef] = p;
    }
    return _rank == _coefficient_size;
}


bool NetworkCodingCODEC::rx(unsigned char* p, const unsigned int pkt_size){
    const NetworkCodibgPacket* pkt = (NetworkCodibgPacket*)p;
    Decoder** decoder = NULL;
    Decoder* new_decoder;
    if((decoder = _decoders.find(pkt->networkcoding_hdr.block_sequence)) == NULL){
        new_decoder = new Decoder(pkt->networkcoding_hdr.coefficient_size);
        decoder = &new_decoder;
        _decoders.insert(pkt->networkcoding_hdr.block_sequence, *decoder);
    }
    return (*decoder)->push(p, pkt_size);
}

int main()
{
    // code = 5
    unsigned char pkt1[9+10];
    unsigned char pkt2[9+10];
    unsigned char pkt3[9+10];
    unsigned char pkt4[9+10];
    unsigned char pkt5[9+10];
    NetworkCodibgPacket* nc_pkt1 = (NetworkCodibgPacket*)pkt1;
    NetworkCodibgPacket* nc_pkt2 = (NetworkCodibgPacket*)pkt2;
    NetworkCodibgPacket* nc_pkt3 = (NetworkCodibgPacket*)pkt3;
    NetworkCodibgPacket* nc_pkt4 = (NetworkCodibgPacket*)pkt4;
    NetworkCodibgPacket* nc_pkt5 = (NetworkCodibgPacket*)pkt5;
    (&nc_pkt1->payload_hdr.coefficients)[0] = 12;
    (&nc_pkt1->payload_hdr.coefficients)[1] = 32;
    (&nc_pkt1->payload_hdr.coefficients)[2] = 105;
    (&nc_pkt1->payload_hdr.coefficients)[3] = 218;
    (&nc_pkt1->payload_hdr.coefficients)[4] = 135;
    (&nc_pkt2->payload_hdr.coefficients)[0] = 23;
    (&nc_pkt2->payload_hdr.coefficients)[1] = 28;
    (&nc_pkt2->payload_hdr.coefficients)[2] = 105;
    (&nc_pkt2->payload_hdr.coefficients)[3] = 218;
    (&nc_pkt2->payload_hdr.coefficients)[4] = 135;
    NetworkCodingCODEC codec;

    return 0;
}
