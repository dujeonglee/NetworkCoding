
#include <iostream>
#include <thread>
#include <atomic>
#include "ncsocket.h"

unsigned int bytes_received = 0;
void rx(unsigned char * buff, unsigned int size, sockaddr_in addr){
    bytes_received += size;
    if((buff[0]^buff[1]) != buff[2])
    {
        exit(-1);
    }
}
int main(int argc, char* argv[])
{
    if(strcmp(argv[0], "./server") == 0)
    {
        printf("start server\n");
        std::thread send_thread_1;

        ncsocket nc_socket(30000, 500, 500, rx);
        send_thread_1 = std::thread([&](){
            unsigned int clientip = 0;
            ((unsigned char*)&clientip)[3] = 127;
            ((unsigned char*)&clientip)[2] = 0;
            ((unsigned char*)&clientip)[1] = 0;
            ((unsigned char*)&clientip)[0] = 1;
            unsigned short int clientport = 30001;

             if(nc_socket.open_session(clientip, clientport, BLOCK_SIZE::SIZE8, 0) == false)
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
                 bytes_sent+=(unsigned int)nc_socket.send(clientip, clientport, data, 1000, false);
             }
             data[0] = data_seq++;
             data[1] = rand()%256;
             data[2] = data[0] ^ data[1];
             bytes_sent+=(unsigned int)nc_socket.send(clientip, clientport, data, 1000, true);
             printf("Complete\n");
         });
         send_thread_1.detach();
         while(1);
    }
    else if(strcmp(argv[0], "./client") == 0)
    {
        printf("start client\n");
        ncsocket nc_socket(30001, 500, 500, rx);
        std::thread rx_thread = std::thread([&](){
            while(1){
                printf("Rx bytes %u\n", bytes_received);
                std::this_thread::sleep_for (std::chrono::milliseconds(1000));
            }
        });

        while(1);
    }
    return 0;
}
