#ifndef SINGLESHOTTIMER_H_
#define SINGLESHOTTIMER_H_
#include <thread>
#include <mutex>

typedef void (*handler_func)();

class singleshottimer{
private:
    std::timed_mutex _lock;
    unsigned int _timeout;
    handler_func _cancel_handler;
    handler_func _timeout_handler;
    std::thread _thread;

public:
    explicit singleshottimer(){};
    ~singleshottimer(){};
    void start(unsigned int timeout_milles, handler_func cancel_handler, handler_func timeout_handler){
        _timeout = timeout_milles;
        _cancel_handler = cancel_handler;
        _timeout_handler = timeout_handler;
        _thread = std::thread([&](){
            this->_lock.lock();
            if(this->_lock.try_lock_for(std::chrono::milliseconds(this->_timeout)) == true){
                if(this->_cancel_handler != nullptr)
                    this->_cancel_handler();
            }else{
                this->_lock.unlock();
                if(this->_timeout_handler != nullptr)
                    this->_timeout_handler();
            }
            return;
        });
    }

    void cancel(){
        _lock.unlock();
    }
};

#endif

#if 0
//Example
#include "singleshottimer.h"
#include <iostream>

void cancel(){
    std::cout << "cancel\n";
}

void timeout(){
    std::cout << "timeout\n";
}

int main(){
    singleshottimer timer;
    std::cout<<"Timeout for: " << 1000 << std::endl;
    timer.start(1000, cancel, timeout);
    std::cout<<"Sleep: " << 100 << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    timer.cancel();
    while(1);
}
#endif
