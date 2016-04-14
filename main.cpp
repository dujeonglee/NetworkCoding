#include <iostream>
#include "ncserver.h"
#include "ncclient.h"

void rx(unsigned char* buff, unsigned int size){
    printf("RX\n");
}

int main()
{
    char buff[10];
    unsigned int ip;
    unsigned short int port;
    ((unsigned char*)&ip)[0] = 50;
    ((unsigned char*)&ip)[1] = 1;
    ((unsigned char*)&ip)[2] = 168;
    ((unsigned char*)&ip)[3] = 192;
    port = 30000;
    ncserver _nc_server(30001, ip, port, 5, 500);
    _nc_server.open_server();
    ncclient _nc_client(port, 5);
    _nc_client.open_client(rx);
    int i = 0;
    do{
        if(_nc_server.send((unsigned char*)buff, 1) == 1){
            i++;
        }
    }while(i < 8);
    std::cout <<"sleep\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    return 0;
}
