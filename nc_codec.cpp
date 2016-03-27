#include "nc_codec.h"

SINGLETON_PATTERN_INITIALIZATION_CPP(NetworkCodingCODEC){}

unsigned char* NetworkCodingCODEC::code(NetworkCodingHeader* hdr, unsigned char* code_length)
{
    (*code_length) = hdr->code_length;
    return &(hdr->code);
}

unsigned char* NetworkCodingCODEC::payload(NetworkCodingHeader* hdr, unsigned char* payload_length)
{
    PayloadHeader* payload_header = (PayloadHeader*)(&(hdr->code) + hdr->code_length);
    (*payload_length) = payload_header->length;
    return &(payload_header->data);
}
