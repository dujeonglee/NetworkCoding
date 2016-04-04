#ifndef SINGLESHOTTIMER_H_
#define SINGLESHOTTIMER_H_
#include <thread>
#include <mutex>
#include <functional>

class singleshottimer{
private:
    std::timed_mutex _lock;
    unsigned int _timeout;
    std::function <void (void)> _cancel_handler;
    std::function <void (void)> _timeout_handler;
    std::thread _thread;
    bool _is_running;

public:
    singleshottimer(){

    };

    ~singleshottimer(){
        while(_is_running);
    };

    void start(unsigned int timeout_milles, std::function <void (void)> cancel_handler, std::function <void (void)> timeout_handler){
        _timeout = timeout_milles;
        _cancel_handler = cancel_handler;
        _timeout_handler = timeout_handler;
        _thread = std::thread([&](){
            _is_running = true;
            this->_lock.lock();
            if(this->_lock.try_lock_for(std::chrono::milliseconds(this->_timeout)) == true){
                if(this->_cancel_handler != nullptr){
                    this->_cancel_handler();
                }
            }else{
                this->_lock.unlock();
                if(this->_timeout_handler != nullptr){
                    this->_timeout_handler();
                }
            }
            _is_running = false;
        });
        _thread.detach();
    }

    void cancel(){
        _lock.unlock();
    }
};

#endif
