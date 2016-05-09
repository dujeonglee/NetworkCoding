#include <string.h>
#include <iostream>
#include "ncserver.h"
#include "ncclient.h"
#include "finite_field.h"

void rx(unsigned char* buff, unsigned int size){
    printf("RX %s", (char*)buff);
}

int main()
{
    unsigned int ip;
    unsigned short int port;
    ((unsigned char*)&ip)[0] = 1;
    ((unsigned char*)&ip)[1] = 0;
    ((unsigned char*)&ip)[2] = 0;
    ((unsigned char*)&ip)[3] = 127;
    port = 30000;
    ncserver _nc_server(30001, ip, port, 5, 500);
    if(_nc_server.open_server() == true)
    {
        std::cout<<"server is ready\n";
    }
    ncclient _nc_client(port, 5);
    if(_nc_client.open_client(rx) == true)
    {
        std::cout<<"client is ready\n";
    }
    int i = 0;
    char data[10];
    do{
        memset(data, 0x0, 10);
        sprintf(data, "%d\n", i);
        if(_nc_server.send((unsigned char*)data, 4+rand()%7, rand()%5 == 0) > 0){
            i++;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }while(true);
    std::cout <<"sleep\n";

    return 0;
}
