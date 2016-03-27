#ifndef NC_CODEC
#define NC_CODEC
#include "design_pattern.h"

const unsigned int max_code_length = 32;

struct NetworkCodingHeader
{
    unsigned short block_seq;
    unsigned char code_length;
    unsigned char code;
}__attribute__((packed)); /*4 bytes*/

struct PayloadHeader
{
    unsigned short length;
    unsigned char data;
}__attribute__((packed)); /*8 bytes*/

class NetworkCodingCODEC{
    SINGLETON_PATTERN_DECLARATION_H(NetworkCodingCODEC)
public:
    static unsigned char* code(NetworkCodingHeader* hdr, unsigned char* code_length);
    static unsigned char* payload(NetworkCodingHeader* hdr, unsigned char* payload_length);

};

#endif
