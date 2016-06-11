
#include <iostream>
#include <thread>
#include <atomic>
#include "ncsocket.h"

unsigned char seq = 0;
unsigned int bytes_received = 0;
void rx_callback_test1(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received += size;
    if(seq != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong: Expect %hhu but get %hhu\n", seq, buffer[0]);
        exit(-1);
    }
    seq++;
}

unsigned char seq_test2[3] = {1,1,1};
unsigned int bytes_received_test2[3] ={0,0,0};
void rx_callback_test2(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    if(buffer[0] > 0){
        bytes_received_test2[0] += size;
        if(seq_test2[0] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
            printf("Something is wrong1: Expect %hhu but get %hhu\n", seq_test2[0], buffer[0]);
            exit(-1);
        }
        seq_test2[0]++;
        if(seq_test2[0] == 0){
            seq_test2[0]++;
        }
    }else if(buffer[3] > 0){
        bytes_received_test2[0] += size;
        if(seq_test2[1] != buffer[3] || buffer[5] != (buffer[3] ^ buffer[4])){
            printf("Something is wrong2: Expect %hhu but get %hhu\n", seq_test2[1], buffer[3]);
            exit(-1);
        }
        seq_test2[1]++;
        if(seq_test2[1] == 0){
            seq_test2[1]++;
        }
    }else{
        bytes_received_test2[2] += size;
        if(seq_test2[2] != buffer[6] || buffer[8] != (buffer[6] ^ buffer[7])){
            printf("Something is wrong3: Expect %hhu but get %hhu\n", seq_test2[2], buffer[6]);
            exit(-1);
        }
        seq_test2[2]++;
        if(seq_test2[2] == 0){
            seq_test2[2]++;
        }
    }
}

unsigned char seq_test3[3] = {0,0,0};
unsigned int bytes_received_test3[3] = {0,0,0};
void rx_callback_test3(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    switch(ntohs(addr.sin_port)){
    case 30000:
        bytes_received_test3[0] += size;
        if(seq_test3[0] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
            printf("Something is wrong1: Expect %hhu but get %hhu\n", seq_test3[0], buffer[0]);
            exit(-1);
        }
        seq_test3[0]++;
        break;
    case 30001:
        bytes_received_test3[1] += size;
        if(seq_test3[1] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
            printf("Something is wrong2: Expect %hhu but get %hhu\n", seq_test3[1], buffer[0]);
            exit(-1);
        }
        seq_test3[1]++;
        break;
    case 30002:
        bytes_received_test3[2] += size;
        if(seq_test3[2] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
            printf("Something is wrong3: Expect %hhu but get %hhu\n", seq_test3[2], buffer[0]);
            exit(-1);
        }
        seq_test3[2]++;
        break;
    default:
        exit(-1);
    }
}

unsigned char seq_test4[3] = {0,0,0};
unsigned int bytes_received_test4[3] = {0,0,0};
void rx_callback_test4_1(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test4[0] += size;
    if(seq_test4[0] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong1: Expect %hhu but get %hhu\n", seq_test4[0], buffer[0]);
        exit(-1);
    }
    seq_test4[0]++;
}

void rx_callback_test4_2(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test4[1] += size;
    if(seq_test4[1] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong2: Expect %hhu but get %hhu\n", seq_test4[1], buffer[0]);
        exit(-1);
    }
    seq_test4[1]++;
}

void rx_callback_test4_3(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test4[2] += size;
    if(seq_test4[2] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong3: Expect %hhu but get %hhu\n", seq_test4[2], buffer[0]);
        exit(-1);
    }
    seq_test4[2]++;
}

unsigned char seq_test6[2] = {0,};
unsigned int bytes_received_test6[2] = {0,};
void rx_callback_test6_1(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test6[0] += size;
    if(seq_test6[0] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong1: Expect %hhu but get %hhu\n", seq_test6[0], buffer[0]);
        exit(-1);
    }
    seq_test6[0]++;
}

void rx_callback_test6_2(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test6[1] += size;
    if(seq_test6[1] != buffer[0] || buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong2: Expect %hhu but get %hhu\n", seq_test6[1], buffer[0]);
        exit(-1);
    }
    seq_test6[1]++;
}

unsigned int bytes_received_test7[2] = {0,};
void rx_callback_test7_1(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test7[0] += size;
    if(buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong1: Expect %hhu but get %hhu\n", seq_test6[0], buffer[0]);
        exit(-1);
    }
    /*
    if(bytes_received_test7[0] % 1000000 == 0){
        printf("%u MBs\n", bytes_received_test7[0] / 1000000);
    }
    */
}

void rx_callback_test7_2(unsigned char* buffer, unsigned int size, sockaddr_in addr){
    bytes_received_test7[1] += size;
    if(buffer[2] != (buffer[0] ^ buffer[1])){
        printf("Something is wrong2: Expect %hhu but get %hhu\n", seq_test6[1], buffer[0]);
        exit(-1);
    }
    /*
    if(bytes_received_test7[1] % 1000000 == 0){
        printf("%u MBs\n", bytes_received_test7[1] / 1000000);
    }
    */
}

int main(int argc, char* argv[])
{
#if 1
    // Test1 One sender sends packets to one receiver
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        ncsocket receiver(30001, 500, 500, rx_callback_test1);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 100000000;
        std::thread sending_thread = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[0]++;
            }
        });
        sending_thread.join();
        std::cout<<"Test1 is passed\n";
    }
    // Test2: One sender running multi threads sends packets to one receiver.
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        ncsocket receiver(30001, 500, 500, rx_callback_test2);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 100000000;
        std::atomic<bool> start;
        start = false;
        std::atomic<bool> thread_1_done;
        thread_1_done = false;
        std::thread sending_thread_1 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[0] = 1;
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[0]++;
                if(data[0] == 0){
                    data[0]++;
                }
            }
            thread_1_done = true;
        });
        std::atomic<bool> thread_2_done;
        thread_2_done = false;
        std::thread sending_thread_2 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[3] = 1;
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[4] = rand()%256;
                data[5] = data[3] ^ data[4];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[3]++;
                if(data[3] == 0){
                    data[3]++;
                }
            }
            thread_2_done = true;
        });
        std::atomic<bool> thread_3_done;
        thread_3_done = false;
        std::thread sending_thread_3 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[6] = 1;
            start = true;
            while(bytes_sent < TEST_SIZE){
                data[7] = rand()%256;
                data[8] = data[6] ^ data[7];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[6]++;
                if(data[6] == 0){
                    data[6]++;
                }
            }
            thread_3_done = true;
        });
        sending_thread_1.detach();
        sending_thread_2.detach();
        sending_thread_3.detach();
        while((thread_1_done == false) || (thread_2_done == false) || (thread_3_done == false));
        std::cout<<"Test2 is passed\n";
    }
    //////////////////////////////////////////////////////////
    // Test3: Multiple senders send packets to one receiver. Each sender uses different block size.
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender_1(30000, 500, 500, nullptr);
        ncsocket sender_2(30001, 500, 500, nullptr);
        ncsocket sender_3(30002, 500, 500, nullptr);
        ncsocket receiver(30003, 500, 500, rx_callback_test3);
        if(sender_1.open_session(clientip, 30003, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(sender_2.open_session(clientip, 30003, BLOCK_SIZE::SIZE4, 0) == false)
        {
            exit(-1);
        }
        if(sender_3.open_session(clientip, 30003, BLOCK_SIZE::SIZE2, 0) == false)
        {
            exit(-1);
        }
        std::atomic<bool> start;
        start = false;
        std::atomic<bool> sender_1_done;
        sender_1_done = false;
        const unsigned int TEST_SIZE = 100000000;
        std::thread sending_thread_1 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender_1.send(clientip, 30003, data, 1000, false);
                data[0]++;
            }
            sender_1_done = true;
        });
        std::atomic<bool> sender_2_done;
        sender_2_done = false;
        std::thread sending_thread_2 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender_2.send(clientip, 30003, data, 1000, false);
                data[0]++;
            }
            sender_2_done = true;
        });
        std::atomic<bool> sender_3_done;
        sender_3_done = false;
        std::thread sending_thread_3 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            start = true;
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender_3.send(clientip, 30003, data, 1000, false);
                data[0]++;
            }
            sender_3_done = true;
        });
        sending_thread_1.detach();
        sending_thread_2.detach();
        sending_thread_3.detach();
        while(sender_1_done == false || sender_2_done == false || sender_3_done == false);
        std::cout<<"Test3 is passed\n";
    }
    // Test4: One sender sends packets to multiple receivers.
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        ncsocket receiver_1(30001, 500, 500, rx_callback_test4_1);
        ncsocket receiver_2(30002, 500, 500, rx_callback_test4_2);
        ncsocket receiver_3(30003, 500, 500, rx_callback_test4_3);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(sender.open_session(clientip, 30002, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(sender.open_session(clientip, 30003, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 300000000;
        std::thread sending_thread = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                bytes_sent += sender.send(clientip, 30002, data, 1000, false);
                bytes_sent += sender.send(clientip, 30003, data, 1000, false);
                data[0]++;
            }
        });
        sending_thread.join();
        std::cout<<"Test4 is passed\n";
    }
    // Test5: One sender running multi threads sends packets to multiple receivers.
    {
        memset(seq_test4, 0, sizeof(seq_test4));
        memset(bytes_received_test4, 0, sizeof(bytes_received_test4));
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, nullptr);
        ncsocket receiver_1(30001, 500, 500, rx_callback_test4_1);
        ncsocket receiver_2(30002, 500, 500, rx_callback_test4_2);
        ncsocket receiver_3(30003, 500, 500, rx_callback_test4_3);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(sender.open_session(clientip, 30002, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(sender.open_session(clientip, 30003, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 100000000;
        std::atomic<bool> thread_1_done;
        thread_1_done = false;
        std::thread sending_thread_1 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[0]++;
            }
            thread_1_done = true;
        });
        std::atomic<bool> thread_2_done;
        thread_2_done = false;
        std::thread sending_thread_2 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30002, data, 1000, false);
                data[0]++;
            }
            thread_2_done = true;
        });
        std::atomic<bool> thread_3_done;
        thread_3_done = false;
        std::thread sending_thread_3 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30003, data, 1000, false);
                data[0]++;
            }
            thread_3_done = true;
        });
        sending_thread_1.detach();
        sending_thread_2.detach();
        sending_thread_3.detach();
        while((thread_1_done == false) || (thread_2_done == false) || (thread_3_done == false));
        std::cout<<"Test5 is passed\n";
    }
    // Test6: Full duplex test
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, rx_callback_test6_1);
        ncsocket receiver(30001, 500, 500, rx_callback_test6_2);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        if(receiver.open_session(clientip, 30000, BLOCK_SIZE::SIZE8, 0) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 100000000;
        std::atomic<bool> start;
        start = false;
        std::atomic<bool> thread_1_done;
        thread_1_done = false;
        std::thread sending_thread_1 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[0] = 0;
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[0]++;
            }
            thread_1_done = true;
        });
        std::atomic<bool> thread_2_done;
        thread_2_done = false;
        std::thread sending_thread_2 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[0] = 0;
            start = true;
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += receiver.send(clientip, 30000, data, 1000, false);
                data[0]++;
            }
            thread_2_done = true;
        });
        sending_thread_1.detach();
        sending_thread_2.detach();
        while((thread_1_done == false) || (thread_2_done == false));
        std::cout<<"Test6 is passed\n";
    }
#endif
    // Test7: Redundancy Test
    {
        unsigned int clientip = 0;
        ((unsigned char*)&clientip)[3] = 127;
        ((unsigned char*)&clientip)[2] = 0;
        ((unsigned char*)&clientip)[1] = 0;
        ((unsigned char*)&clientip)[0] = 1;

        ncsocket sender(30000, 500, 500, rx_callback_test7_1);
        ncsocket receiver(30001, 500, 500, rx_callback_test7_2);
        if(sender.open_session(clientip, 30001, BLOCK_SIZE::SIZE8, 0, 4) == false)
        {
            exit(-1);
        }
        if(receiver.open_session(clientip, 30000, BLOCK_SIZE::SIZE8, 0, 4) == false)
        {
            exit(-1);
        }
        const unsigned int TEST_SIZE = 100000000;
        std::atomic<bool> start;
        start = false;
        std::atomic<bool> thread_1_done;
        thread_1_done = false;
        std::thread sending_thread_1 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[0] = 0;
            while(start == false);
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += sender.send(clientip, 30001, data, 1000, false);
                data[0]++;
            }
            thread_1_done = true;
        });
        std::atomic<bool> thread_2_done;
        thread_2_done = false;
        std::thread sending_thread_2 = std::thread([&](){
            unsigned int bytes_sent = 0;
            unsigned char data[1000] = {0};
            data[0] = 0;
            start = true;
            while(bytes_sent < TEST_SIZE){
                data[1] = rand()%256;
                data[2] = data[0] ^ data[1];
                bytes_sent += receiver.send(clientip, 30000, data, 1000, false);
                data[0]++;
            }
            thread_2_done = true;
        });
        sending_thread_1.detach();
        sending_thread_2.detach();
        while((thread_1_done == false) || (thread_2_done == false));
        sleep(1);
        std::cout<<"Test7 is passed\n";
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
    return 0;
}
