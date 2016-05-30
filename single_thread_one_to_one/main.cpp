#include <string.h>
#include <iostream>
#include <thread>
#include "ncserver.h"
#include "ncclient.h"
#include "finite_field.h"

unsigned int bytes_received = 0;
unsigned char data_recv_seq = 0;
void rx(unsigned char * buff, unsigned int size){
    bytes_received += size;
    if(buff[0] != data_recv_seq)
    {
        printf("Something is wrong %hhu VS %hhu %u\n", buff[0], data_recv_seq, size);
        exit(-1);
    }
    data_recv_seq++;
    if(bytes_received%1000000 == 0)
    {
        printf("%f MB\n", (float)bytes_received/1000000.);
    }
}

int main(int argc, char* argv[])
{
    if(strcmp(argv[0], "./server") == 0)
    {
        printf("start server\n");
        unsigned short int serverport;
        unsigned int clientip;
        unsigned short int clientport;
        sscanf(argv[1], "%hu", &serverport);
        sscanf(argv[2], "%hhu.%hhu.%hhu.%hhu", ((unsigned char*)&clientip)+3, ((unsigned char*)&clientip)+2, ((unsigned char*)&clientip)+1, ((unsigned char*)&clientip));
        sscanf(argv[3], "%hu", &clientport);

        printf("%hu %hhu.%hhu.%hhu.%hhu %hu\n", serverport, ((unsigned char*)&clientip)[3], ((unsigned char*)&clientip)[2], ((unsigned char*)&clientip)[1], ((unsigned char*)&clientip)[0], clientport);

        ncserver nc_server(serverport, clientip, clientport, SIZE8, 10);
        if(nc_server.open_server() == false)
        {
            exit(-1);
        }
        unsigned char data[1000] = {0,};
        unsigned int bytes_sent = 0;
        unsigned char data_seq = 0;
        while(bytes_sent < 99999000)
        {
            data[0] = data_seq++;
            bytes_sent+=(unsigned int)nc_server.send(data, 1000, false);
        }
        data[0] = data_seq++;
        bytes_sent+=(unsigned int)nc_server.send(data, 1000, true);
        printf("Complete\n");
    }
    else if(strcmp(argv[0], "./client") == 0)
    {
        printf("start client\n");
        unsigned short int clientport;
        sscanf(argv[1], "%hu", &clientport);
        ncclient nc_client(clientport, SIZE8);
        if(nc_client.open_client(rx) == false)
        {
            exit(-1);
        }
        while(bytes_received < 100000000);
        sleep(1);
        printf("Complete\n");
    }
    return 0;
}
