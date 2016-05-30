#include <string.h>
#include <iostream>
#include <thread>
#include <atomic>
#include "ncserver.h"
#include "ncclient.h"
#include "finite_field.h"

unsigned int bytes_received = 0;
unsigned char data_recv_seq = 0;
void rx(unsigned char * buff, unsigned int size){
    bytes_received += size;
    if((buff[0]^buff[1]) != buff[2])
    {
        printf("Something is wrong %hhu VS %hhu %u\n", buff[0], data_recv_seq, size);
        exit(-1);
    }else{
        //printf("RECV %hhu [%u]\n", buff[0], size);
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
        std::thread send_thread_1;
        std::thread send_thread_2;
        sscanf(argv[1], "%hu", &serverport);
        sscanf(argv[2], "%hhu.%hhu.%hhu.%hhu", ((unsigned char*)&clientip)+3, ((unsigned char*)&clientip)+2, ((unsigned char*)&clientip)+1, ((unsigned char*)&clientip));
        sscanf(argv[3], "%hu", &clientport);
        std::atomic<unsigned char> finished;
        finished = 0;

        ncserver nc_server(serverport, 10);

         send_thread_1 = std::thread([&](){
             if(nc_server.open_session(clientip, clientport, BLOCK_SIZE::SIZE8) == false)
             {
                 exit(-1);
             }
             unsigned char data[1000] = {0,};
             unsigned int bytes_sent = 0;
             unsigned char data_seq = 0;
             while(bytes_sent < 99999000)
             {
                 data[0] = data_seq++;
                 data[1] = rand()%256;
                 data[2] = data[0] ^ data[1];
                 bytes_sent+=(unsigned int)nc_server.send(clientip, clientport, data, 1000, false);
             }
             data[0] = data_seq++;
             data[1] = rand()%256;
             data[2] = data[0] ^ data[1];
             bytes_sent+=(unsigned int)nc_server.send(clientip, clientport, data, 1000, true);
             printf("Complete\n");
             finished++;
         });
         send_thread_1.detach();
         send_thread_2 = std::thread([&](){
             /*
             if(nc_server.open_session(clientip, clientport+1, BLOCK_SIZE::SIZE8) == false)
             {
                 exit(-1);
             }
             */
             unsigned char data[1000] = {0,};
             unsigned int bytes_sent = 0;
             unsigned char data_seq = 0;
             while(bytes_sent < 99999000)
             {
                 data[0] = data_seq++;
                 data[1] = rand()%256;
                 data[2] = data[0] ^ data[1];
                 bytes_sent+=(unsigned int)nc_server.send(clientip, clientport, data, 1000, false);
             }
             data[0] = data_seq++;
             data[1] = rand()%256;
             data[2] = data[0] ^ data[1];
             bytes_sent+=(unsigned int)nc_server.send(clientip, clientport, data, 1000, true);
             printf("Complete\n");
             finished++;
         });
         send_thread_2.detach();
         while(finished != 2);
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
        while(bytes_received < 200000000);
        sleep(1);
        printf("Complete\n");
    }
    return 0;
}
