#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <vector>
#include "ncsocket.h"

void TC0()
{
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    unsigned char tx_buffer[1024] = {0,};

    FILE* readfile = fopen("./Canon.mp3", "r");
    FILE* writefile = fopen("./out.mp3", "w");
    if(readfile == nullptr || writefile == nullptr)
    {
        exit(-1);
    }


    ncsocket sender(htons(20000), 500, 500, nullptr);
    ncsocket receiver(htons(20001), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr)
    {
        fwrite (buffer, 1, size, writefile);
    });

    if(sender.open_session(clientip, htons(20001), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    std::thread sending_thread = std::thread([&](){
        int read = 0;
        while((read = fread (tx_buffer, 1,1024, readfile)) > 0)
        {
            sender.send(clientip, htons(20001), tx_buffer, read, (read!=1024));
        }
    });
    sending_thread.join();
    sleep(1);
    fclose(readfile);
    fclose(writefile);
}

void TC1()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::vector<unsigned char> tx_random_data;
    std::vector<unsigned int> tx_random_size;
    std::vector<unsigned char> rx_random_data;
    std::vector<unsigned int> rx_random_size;
    unsigned char tx_buffer[1024] = {0,};
    unsigned int received = 0;

    for(unsigned int cases = 0 ; cases < TOTAL_CASES ; cases++)
    {
        tx_random_data.push_back(rand()%256);
        tx_random_size.push_back((rand()%1024)+1);
    }

    ncsocket sender(htons(30000), 500, 500, nullptr);
    ncsocket receiver(htons(30001), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr)
    {
        rx_random_data.push_back(buffer[0]);
        rx_random_size.push_back(size);
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC1 Random packet size with random retransmission triggering"<<std::flush;
        }
    });

    if(sender.open_session(clientip, htons(30001), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    std::thread sending_thread = std::thread([&](){
        for(unsigned int cases = 0 ; cases < TOTAL_CASES ; cases++)
        {
            const unsigned int size = tx_random_size[cases];
            memset(tx_buffer, tx_random_data[cases], size);
            if(size != (unsigned int)sender.send(clientip, htons(30001), tx_buffer, size, ((rand()%4) == 0 || cases == TOTAL_CASES-1)))
            {
                cases--;
            }
        }
    });
    sending_thread.join();
    usleep(1000);
    for(unsigned int cases = 0 ; cases < TOTAL_CASES ; cases++)
    {
        if((tx_random_data[cases] != rx_random_data[cases]) || (tx_random_size[cases] != rx_random_size[cases]))
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC1 Random packet size with random retransmission triggering"<<"...[NG]\n";
            exit(-1);
        }
    }
    std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC1 Random packet size with random retransmission triggering"<<"...[OK]\n";
}

void TC2()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::atomic<unsigned int> received;
    received = 0;

    unsigned char host_1_expect = 0;
    unsigned char host_2_expect = 0;
    bool host_1_pkt_order_preserved = true;
    bool host_2_pkt_order_preserved = true;

    ncsocket host_1(htons(30002), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr)
    {
        if(buffer[0] != host_1_expect++)
        {
            host_1_pkt_order_preserved = false;
        }
        received++;
        if((received%(TOTAL_CASES/100)) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC2 Full-duplex communication"<<std::flush;
        }
    });

    ncsocket host_2(htons(30003), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr)
    {
        if(buffer[0] != host_2_expect++)
        {
            host_2_pkt_order_preserved = false;
        }
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC2 Full-duplex communication"<<std::flush;
        }
    });

    if(host_1.open_session(clientip, htons(30003), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    if(host_2.open_session(clientip, htons(30002), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }

    std::atomic<bool> send_start;
    send_start = false;
    unsigned char host_1_data = 0;
    std::thread host_1_send = std::thread([&](){
        unsigned char data[1000] = {0};
        while(send_start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/2 ; cases++)
        {
            data[0] = host_1_data++;
            const int ret = host_1.send(clientip, htons(30003), data, 1000, false);
            if(ret != 1000)
            {
                cases--;
            }
        }
    });

    unsigned char host_2_data = 0;
    std::thread host_2_send = std::thread([&](){
        unsigned char data[1000] = {0};
        while(send_start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/2 ; cases++)
        {
            data[0] = host_2_data++;
            const int ret = host_2.send(clientip, htons(30002), data, 1000, false);
            if(ret != 1000)
            {
                cases--;
            }
        }
    });
    send_start = true;
    host_1_send.detach();
    host_2_send.detach();
    while(received < TOTAL_CASES);
    usleep(1000);
    if(host_1_pkt_order_preserved && host_2_pkt_order_preserved)
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC2 Full-duplex communication"<<"...[OK]\n";
    }
    else
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC2 Full-duplex communication"<<"...[NG]\n";
        exit(-1);
    }
}

void TC3()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::atomic<unsigned int> received;
    received = 0;

    unsigned char thread_1_expect = 1;
    unsigned char thread_2_expect = 1;
    unsigned char thread_3_expect = 1;
    unsigned char thread_4_expect = 1;
    bool thread_1_pkt_order_preserved = true;
    bool thread_2_pkt_order_preserved = true;
    bool thread_3_pkt_order_preserved = true;
    bool thread_4_pkt_order_preserved = true;

    ncsocket sender(htons(30004), 500, 500, nullptr);
    ncsocket receiver(htons(30005), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC3 Multi-thread"<<std::flush;
        }
        if(buffer[0])
        {
            if(buffer[0] != thread_1_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_1_pkt_order_preserved = false;
            }
            thread_1_expect++;
            if(thread_1_expect == 0)
            {
                thread_1_expect = 1;
            }
        }
        if(buffer[3])
        {
            if(buffer[3] != thread_2_expect || buffer[5] != (buffer[3] ^ buffer[4]))
            {
                thread_2_pkt_order_preserved = false;
            }
            thread_2_expect++;
            if(thread_2_expect == 0)
            {
                thread_2_expect = 1;
            }
        }
        if(buffer[6])
        {
            if(buffer[6] != thread_3_expect || buffer[8] != (buffer[6] ^ buffer[7]))
            {
                thread_3_pkt_order_preserved = false;
            }
            thread_3_expect++;
            if(thread_3_expect == 0)
            {
                thread_3_expect = 1;
            }
        }
        if(buffer[9])
        {
            if(buffer[9] != thread_4_expect || buffer[11] != (buffer[9] ^ buffer[10]))
            {
                thread_4_pkt_order_preserved = false;
            }
            thread_4_expect++;
            if(thread_4_expect == 0)
            {
                thread_4_expect = 1;
            }
        }
    });
    if(sender.open_session(clientip, htons(30005), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    std::atomic<bool> start;
    start = false;

    std::thread thread_1 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            const int ret = sender.send(clientip, htons(30005), data, 1000, cases == (TOTAL_CASES/4-1));
            if(ret != 1000)
            {
                cases--;
            }
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    std::thread thread_2 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[3] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[4] = rand()%256;
            data[5] = data[3] ^ data[4];
            sender.send(clientip, htons(30005), data, 1000, cases == (TOTAL_CASES/4-1));
            data[3]++;
            if(data[3] == 0)
            {
                data[3] = 1;
            }
        }
    });
    std::thread thread_3 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[6] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[7] = rand()%256;
            data[8] = data[6] ^ data[7];
            sender.send(clientip, htons(30005), data, 1000, cases == (TOTAL_CASES/4-1));
            data[6]++;
            if(data[6] == 0)
            {
                data[6] = 1;
            }
        }
    });
    std::thread thread_4 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[9] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[10] = rand()%256;
            data[11] = data[9] ^ data[10];
            sender.send(clientip, htons(30005), data, 1000, cases == (TOTAL_CASES/4-1));
            data[9]++;
            if(data[9] == 0)
            {
                data[9] = 1;
            }
        }
    });
    start = true;
    thread_1.detach();
    thread_2.detach();
    thread_3.detach();
    thread_4.detach();
    while(received < TOTAL_CASES);
    usleep(1000);
    if(thread_1_pkt_order_preserved && thread_2_pkt_order_preserved && thread_3_pkt_order_preserved && thread_4_pkt_order_preserved)
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC3 Multi-thread"<<"...[OK]\n";
    }
    else
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC3 Multi-thread"<<"...[NG]\n";
        exit(-1);
    }
}

void TC4()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::atomic<unsigned int> received;
    received = 0;

    unsigned int rx[4] = {0,};

    unsigned char thread_1_expect = 1;
    unsigned char thread_2_expect = 1;
    unsigned char thread_3_expect = 1;
    unsigned char thread_4_expect = 1;
    bool thread_1_pkt_order_preserved = true;
    bool thread_2_pkt_order_preserved = true;
    bool thread_3_pkt_order_preserved = true;
    bool thread_4_pkt_order_preserved = true;

    ncsocket sender_1(htons(30006), 500, 500, nullptr);
    ncsocket sender_2(htons(30007), 500, 500, nullptr);
    ncsocket sender_3(htons(30008), 500, 500, nullptr);
    ncsocket sender_4(htons(30009), 500, 500, nullptr);
    ncsocket receiver(htons(30010), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC4 Multi-senders"<<std::flush;
        }
        if(buffer[0])
        {
            rx[0]++;
            if(buffer[0] != thread_1_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_1_pkt_order_preserved = false;
            }
            thread_1_expect++;
            if(thread_1_expect == 0)
            {
                thread_1_expect = 1;
            }
        }
        if(buffer[3])
        {
            rx[1]++;
            if(buffer[3] != thread_2_expect || buffer[5] != (buffer[3] ^ buffer[4]))
            {
                thread_2_pkt_order_preserved = false;
            }
            thread_2_expect++;
            if(thread_2_expect == 0)
            {
                thread_2_expect = 1;
            }
        }
        if(buffer[6])
        {
            rx[2]++;
            if(buffer[6] != thread_3_expect || buffer[8] != (buffer[6] ^ buffer[7]))
            {
                thread_3_pkt_order_preserved = false;
            }
            thread_3_expect++;
            if(thread_3_expect == 0)
            {
                thread_3_expect = 1;
            }
        }
        if(buffer[9])
        {
            rx[3]++;
            if(buffer[9] != thread_4_expect || buffer[11] != (buffer[9] ^ buffer[10]))
            {
                thread_4_pkt_order_preserved = false;
            }
            thread_4_expect++;
            if(thread_4_expect == 0)
            {
                thread_4_expect = 1;
            }
        }
    });
    if(sender_1.open_session(clientip, htons(30010), BLOCK_SIZE::SIZE2, 0) == false)
    {
        exit(-1);
    }
    if(sender_2.open_session(clientip, htons(30010), BLOCK_SIZE::SIZE4, 0) == false)
    {
        exit(-1);
    }
    if(sender_3.open_session(clientip, htons(30010), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    if(sender_4.open_session(clientip, htons(30010), BLOCK_SIZE::SIZE16, 0) == false)
    {
        exit(-1);
    }
    std::atomic<bool> start;
    start = false;

    std::thread thread_1 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            if(sender_1.send(clientip, htons(30010), data, 1000, cases == (TOTAL_CASES/4-1)) != 1000)
            {
                std::cout<<"[1]Send is failed\n";
                exit(-1);
            }
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    std::thread thread_2 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[3] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[4] = rand()%256;
            data[5] = data[3] ^ data[4];
            if(sender_2.send(clientip, htons(30010), data, 1000, cases == (TOTAL_CASES/4-1)) != 1000)
            {
                std::cout<<"[2]Send is failed\n";
                exit(-1);
            }
            data[3]++;
            if(data[3] == 0)
            {
                data[3] = 1;
            }
        }
    });
    std::thread thread_3 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[6] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[7] = rand()%256;
            data[8] = data[6] ^ data[7];
            if(sender_3.send(clientip, htons(30010), data, 1000, cases == (TOTAL_CASES/4-1)) != 1000)
            {
                std::cout<<"[4]Send is failed\n";
                exit(-1);
            }
            data[6]++;
            if(data[6] == 0)
            {
                data[6] = 1;
            }
        }
    });
    std::thread thread_4 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[9] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[10] = rand()%256;
            data[11] = data[9] ^ data[10];
            if(sender_4.send(clientip, htons(30010), data, 1000, cases == (TOTAL_CASES/4-1)) != 1000)
            {
                std::cout<<"[4]Send is failed\n";
                exit(-1);
            }

            data[9]++;
            if(data[9] == 0)
            {
                data[9] = 1;
            }
        }
    });
    start = true;
    thread_1.detach();
    thread_2.detach();
    thread_3.detach();
    thread_4.detach();
    while(received < TOTAL_CASES);
    usleep(1000);
    if(thread_1_pkt_order_preserved && thread_2_pkt_order_preserved && thread_3_pkt_order_preserved && thread_4_pkt_order_preserved)
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC4 Multi-senders"<<"...[OK]\n";
    }
    else
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC4 Multi-senders"<<"...[NG]\n";
        exit(-1);
    }
}

void TC5()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::atomic<unsigned int> received;
    received = 0;

    unsigned char thread_1_expect = 1;
    unsigned char thread_2_expect = 1;
    unsigned char thread_3_expect = 1;
    unsigned char thread_4_expect = 1;
    bool thread_1_pkt_order_preserved = true;
    bool thread_2_pkt_order_preserved = true;
    bool thread_3_pkt_order_preserved = true;
    bool thread_4_pkt_order_preserved = true;

    ncsocket sender(htons(30011), 500, 500, nullptr);
    ncsocket receiver_1(htons(30012), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receivers"<<std::flush;
        }
        if(buffer[0])
        {
            if(buffer[0] != thread_1_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_1_pkt_order_preserved = false;
            }
            thread_1_expect++;
            if(thread_1_expect == 0)
            {
                thread_1_expect = 1;
            }
        }
    });
    ncsocket receiver_2(htons(30013), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receivers"<<std::flush;
        }
        if(buffer[0])
        {
            if(buffer[0] != thread_2_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_2_pkt_order_preserved = false;
            }
            thread_2_expect++;
            if(thread_2_expect == 0)
            {
                thread_2_expect = 1;
            }
        }
    });
    ncsocket receiver_3(htons(30014), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receivers"<<std::flush;
        }
        if(buffer[0])
        {
            if(buffer[0] != thread_3_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_3_pkt_order_preserved = false;
            }
            thread_3_expect++;
            if(thread_3_expect == 0)
            {
                thread_3_expect = 1;
            }
        }
    });
    ncsocket receiver_4(htons(30015), 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        received++;
        if(received%(TOTAL_CASES/100) == 0)
        {
            std::cout<<"\r["<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receivers"<<std::flush;
        }
        if(buffer[0])
        {
            if(buffer[0] != thread_4_expect || buffer[2] != (buffer[0] ^ buffer[1]))
            {
                thread_4_pkt_order_preserved = false;
            }
            thread_4_expect++;
            if(thread_4_expect == 0)
            {
                thread_4_expect = 1;
            }
        }
    });
    if(sender.open_session(clientip, htons(30012), BLOCK_SIZE::SIZE2, 0) == false)
    {
        exit(-1);
    }
    if(sender.open_session(clientip, htons(30013), BLOCK_SIZE::SIZE4, 0) == false)
    {
        exit(-1);
    }
    if(sender.open_session(clientip, htons(30014), BLOCK_SIZE::SIZE8, 0) == false)
    {
        exit(-1);
    }
    if(sender.open_session(clientip, htons(30015), BLOCK_SIZE::SIZE16, 0) == false)
    {
        exit(-1);
    }
    std::atomic<bool> start;
    start = false;

    std::thread thread_1 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            sender.send(clientip, htons(30012), data, 1000, cases == (TOTAL_CASES/4-1));
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    std::thread thread_2 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            sender.send(clientip, htons(30013), data, 1000, cases == (TOTAL_CASES/4-1));
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    std::thread thread_3 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            sender.send(clientip, htons(30014), data, 1000, cases == (TOTAL_CASES/4-1));
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    std::thread thread_4 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 1;
        while(start == false);
        for(unsigned int cases = 0 ; cases < TOTAL_CASES/4 ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            sender.send(clientip, htons(30015), data, 1000, cases == (TOTAL_CASES/4-1));
            data[0]++;
            if(data[0] == 0)
            {
                data[0] = 1;
            }
        }
    });
    start = true;
    thread_1.detach();
    thread_2.detach();
    thread_3.detach();
    thread_4.detach();
    while(received < TOTAL_CASES);
    usleep(1000);
    if(thread_1_pkt_order_preserved && thread_2_pkt_order_preserved && thread_3_pkt_order_preserved && thread_4_pkt_order_preserved)
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receiver"<<"...[OK]\n";
    }
    else
    {
        std::cout<<"\r["<<std::flush<<std::setw(3)<<received/(TOTAL_CASES/100)<<"%]"<<"TC5 Multi-receiver"<<"...[NG]\n";
        exit(-1);
    }
}

void TC6()
{
    const unsigned int TOTAL_CASES = 100000;
    unsigned int clientip = 0;
    ((unsigned char*)&clientip)[0] = 127;
    ((unsigned char*)&clientip)[1] = 0;
    ((unsigned char*)&clientip)[2] = 0;
    ((unsigned char*)&clientip)[3] = 1;

    std::atomic<unsigned int> sent;
    sent = 0;

    bool decoding_error_1 = false;
    bool decoding_error_2 = false;
    unsigned int received_1 = 0;
    unsigned int received_2 = 0;

    ncsocket host1(30000, 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        if(buffer[2] != (buffer[0] ^ buffer[1]))
        {
            decoding_error_1 = true;
        }
        received_1++;
    });
    ncsocket host2(30001, 500, 500, [&](unsigned char* buffer, unsigned int size, sockaddr_in addr){
        if(buffer[2] != (buffer[0] ^ buffer[1]))
        {
            decoding_error_2 = true;
        }
        received_2++;
    });
    if(host1.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0, 4) == false)
    {
        exit(-1);
    }
    if(host2.open_session(clientip, 30000, BLOCK_SIZE::SIZE4, 0, 2) == false)
    {
        exit(-1);
    }
    std::atomic<bool> start;
    start = false;
    std::thread thread_1 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 0;
        while(start == false);
        for(unsigned int cases = 0 ; cases < (TOTAL_CASES/2) ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            host1.send(clientip, 30001, data, 1000, false);
            data[0]++;
            sent++;
            if(sent%(TOTAL_CASES/100) == 0)
            {
                std::cout<<"\r["<<std::setw(3)<<sent/(TOTAL_CASES/100)<<"%]"<<"TC6 Best-effort"<<std::flush;
            }
            if(data[0]%10 == 0)
            {
                usleep(200);
            }
        }
    });
    std::thread thread_2 = std::thread([&](){
        unsigned char data[1000] = {0};
        data[0] = 0;
        while(start == false);
        for(unsigned int cases = 0 ; cases < (TOTAL_CASES/2) ; cases++)
        {
            data[1] = rand()%256;
            data[2] = data[0] ^ data[1];
            host2.send(clientip, 30000, data, 1000, false);
            data[0]++;
            sent++;
            if(sent%(TOTAL_CASES/100) == 0)
            {
                std::cout<<"\r["<<std::setw(3)<<sent/(TOTAL_CASES/100)<<"%]"<<"TC6 Best-effort"<<std::flush;
            }
            if(data[0]%10 == 0)
            {
                usleep(200);
            }
        }
    });
    start = true;
    thread_1.detach();
    thread_2.detach();
    while(sent < TOTAL_CASES);
    usleep(1000);
    if(decoding_error_1 == false && decoding_error_2 == false)
    {
        std::cout<<"\r["<<std::setw(3)<<sent/(TOTAL_CASES/100)<<"%]"<<"TC6 Best-effort...[OK]";
    }
    else
    {
        std::cout<<"\r["<<std::setw(3)<<sent/(TOTAL_CASES/100)<<"%]"<<"TC6 Best-effort...[NG]";
    }
    std::cout<<"Host1: "<<received_1<<"/50000 / Host2: "<<received_2<<"/50000\n";
}

int main(int argc, char* argv[])
{
    TC0();
    TC1();
    TC2();
    TC3();
    TC4();
    TC5();
    TC6();
#if 0
    // Test7: Redundancy Test
    {
    }
    // Test 8 connection test
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0, 4) == false)
        {
            exit(-1);
        }
        if(sender.connect_session(clientip, 30001, 3, 500) == true)
        {
            std::cout<<"connection established\n";
            exit(-1);
        }
        else
        {
            std::cout<<"connection is not established\n";
        }
        ncsocket receiver(30001, 500, 500, nullptr);
        if(sender.connect_session(clientip, 30001, 3, 500) == true)
        {
            std::cout<<"connection established\n";
        }
        else
        {
            std::cout<<"connection is not established\n";
            exit(-1);
        }
        std::cout<<"Test 8 is passed\n";
    }
    // Test 9 connection test
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 50000000;
        std::atomic<bool> thread_1_done;
        thread_1_done = false;
        std::thread sending_thread_1 = std::thread([&](){
            {
                ncsocket receiver(30001, 500, 500, nullptr);
                unsigned int bytes_sent = 0;
                unsigned char data[1000] = {0};
                data[0] = 0;
                while(bytes_sent < TEST_SIZE){
                    data[1] = rand()%256;
                    data[2] = data[0] ^ data[1];
                    bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                    data[0]++;
                }
            }
            {
                unsigned int bytes_sent = 0;
                unsigned char data[1000] = {0};
                data[0] = 0;
                while(bytes_sent < TEST_SIZE){
                    int ret=0;
                    data[1] = rand()%256;
                    data[2] = data[0] ^ data[1];
                    bytes_sent += (ret = sender.send(clientip, 30001, data, 1000, false));
                    if(ret == 0)
                    {
                        std::cout<<"Connection is lost\n";
                        break;
                    }
                    data[0]++;
                }
            }
            thread_1_done = true;
        });
        sending_thread_1.detach();
        while((thread_1_done == false));
        sleep(1);
        std::cout<<"Test 9 is passed\n";
    }
#endif
    return 0;
}
