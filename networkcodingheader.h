#ifndef NETWORKCODINGHEADER_H_
#define NETWORKCODINGHEADER_H_

#include <stddef.h>

#define OUTER_HEADER_SIZE                                              (sizeof(OuterHeader))
#define GET_OUTER_TYPE(pkt)                                             (((OuterHeader*)pkt)->type)
#define GET_OUTER_SIZE(pkt)                                             (((OuterHeader*)pkt)->size)
#define GET_OUTER_BLK_SEQ(pkt)                                     (((OuterHeader*)pkt)->blk_seq)
#define GET_OUTER_BLK_SIZE(pkt)                                     (((OuterHeader*)pkt)->blk_size)
#define GET_OUTER_MAX_BLK_SIZE(pkt)                           (((OuterHeader*)pkt)->max_blk_size)
#define GET_OUTER_FLAGS(pkt)                                         (((OuterHeader*)pkt)->flags)
#define GET_INNER_SIZE(pkt)                                              (((InnerHeader*)((unsigned char*)pkt + sizeof(OuterHeader)))->size)
#define GET_INNER_LAST_INDICATOR(pkt)                        (((InnerHeader*)((unsigned char*)pkt + sizeof(OuterHeader)))->last_indicator)
#define GET_INNER_CODE(pkt)                                           (((InnerHeader*)((unsigned char*)pkt + sizeof(OuterHeader)))->codes)
#define GET_INNER_PAYLOAD(pkt, max_block_size)         ((unsigned char*)pkt + sizeof(OuterHeader) + sizeof(InnerHeader) + (max_block_size-1))

#define ETHERNET_MTU                                                      (1500)
#define TOTAL_HEADER_SIZE(max_block_size)                  (sizeof(OuterHeader) + sizeof(InnerHeader) + (max_block_size-1))
#define MAX_PAYLOAD_SIZE(max_block_size)                  (ETHERNET_MTU - 20/*IP*/ - 8/*UDP*/ - TOTAL_HEADER_SIZE(max_block_size))
#define MAX_BUFFER_SIZE                                                  (ETHERNET_MTU - 20 - 8)

enum NC_PKT_TYPE : unsigned char{
    DATA_TYPE = 0,
    ACK_TYPE,
    REQ_CONNECT_TYPE,
    REP_CONNECT_TYPE
};

struct OuterHeader
{
    unsigned char type;
    unsigned short int size;          /*2*/
    unsigned short int blk_seq;    /*2*/
    unsigned char blk_size;          /*1*/
    unsigned char max_blk_size; /*1*/
    unsigned char flags;               /*1*/
    enum : unsigned char
    {
        FLAGS_ORIGINAL = 0x1,
        FLAGS_END_OF_BLK = 0x2
    };
}__attribute__((packed));

#define PRINT_OUTERHEADER(pkt)\
do\
{\
    printf("OUTERHDR [Size = %6hu][Block Seq = %6hu][Block Size = %3hhu][Flags = %hhx]\n", \
    ((OuterHeader*)pkt)->size, ((OuterHeader*)pkt)->blk_seq, ((OuterHeader*)pkt)->blk_size, ((OuterHeader*)pkt)->flags);\
}while(0)

struct InnerHeader
{
    unsigned short int size;          /*2*/
    unsigned char last_indicator;	/*1*/
    unsigned char codes[1];        /*blk_size*/
}__attribute__((packed));

#define PRINT_INNERHEADER(max_block_size, pkt)\
do\
{\
    printf("INNERHDR [Size = %6hu][Last Indicator = %2hhu][Code =", ((InnerHeader*)(pkt+sizeof(OuterHeader)))->size, ((InnerHeader*)(pkt+sizeof(OuterHeader)))->last_indicator);\
    for(unsigned char _tmp = 0 ; _tmp < max_block_size ; _tmp++)\
    {\
        printf(" %hhu", ((InnerHeader*)(pkt+sizeof(OuterHeader)))->codes[_tmp]);\
    }\
    printf("]\n");\
}while(0)


#define PRINT_CODE_BLOCK(max_block_size, buf)\
do\
{\
    printf("Code===\n");\
    for(unsigned char _tmp2 = 0 ; _tmp2 < max_block_size ; _tmp2++)\
    {\
        for(unsigned char _tmp3 = 0 ; _tmp3 < max_block_size ; _tmp3++)\
        {\
            printf("%4hhu", GET_INNER_CODE(buf[_tmp2].buffer)[_tmp3]);\
        }\
        printf("\n");\
    }\
}while(0)

struct Ack
{
    unsigned char type;
    unsigned short int blk_seq;    /*2*/
    unsigned char losses;             /*1*/
}__attribute__((packed));

struct Connect
{
    unsigned char type;
}__attribute__((packed));

enum BLOCK_SIZE: unsigned char
{
    SIZE2 = 0x1<<1,
    SIZE4 = 0x1<<2,
    SIZE8 = 0x1<<3,
    SIZE16 = 0x1<<4
};

struct NetworkCodingPktBuffer
{
    unsigned char buffer[MAX_BUFFER_SIZE];
};

// Key of tx_session_info for avltree
struct ip_port_key{
    unsigned int ip;
    unsigned short port;
}__attribute__((packed));
#endif
