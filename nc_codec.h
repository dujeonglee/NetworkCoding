#ifndef NC_CODEC
#define NC_CODEC
#include "design_pattern.h"
#include "avltree.h"
#include <vector>

const unsigned int max_code_length = 32;


struct NetworkCodingHeader
{
    unsigned short block_sequence;
    unsigned char coefficient_size;
}__attribute__((packed)); /*4 bytes*/

struct PayloadHeader
{
    unsigned short int payload_size;
    unsigned char coefficients;
}__attribute__((packed)); /*8 bytes*/

struct NetworkCodibgPacket{
    NetworkCodingHeader networkcoding_hdr;
    PayloadHeader payload_hdr;
}__attribute__((packed));

class Decoder{
private:
    unsigned char _coefficient_size;
    std::vector<unsigned char*> _buffer;
    unsigned char _rank;
public:
    Decoder(unsigned char);
    ~Decoder();

    bool push(unsigned char* pkt, const unsigned int pkt_size);
};

class NetworkCodingCODEC{
    SINGLETON_PATTERN_DECLARATION_H(NetworkCodingCODEC)
    avltree<unsigned int, Decoder*> _decoders;
public:
    bool rx(unsigned char* pkt, const unsigned int pkt_size);

};

#endif
