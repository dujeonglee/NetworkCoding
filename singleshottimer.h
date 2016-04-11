#ifndef SINGLESHOTTIMER_H_
#define SINGLESHOTTIMER_H_
#include <thread>
#include <mutex>
#include <functional>

class singleshottimer{
private:
    std::timed_mutex _timer_lock;
    std::mutex _handler_lock;
    unsigned int _timeout;
    std::function <void (void)> _cancel_handler;
    std::function <void (void)> _timeout_handler;
    std::thread _thread;
    bool _is_running;

public:
    explicit singleshottimer(){};
    ~singleshottimer(){
        _handler_lock.lock();
        _cancel_handler = nullptr;
        _timeout_handler = nullptr;
        _handler_lock.unlock();
        while(_is_running);
    };
    void start(unsigned int timeout_milles, std::function <void (void)> cancel_handler, std::function <void (void)> timeout_handler){
        _timeout = timeout_milles;
        _cancel_handler = cancel_handler;
        _timeout_handler = timeout_handler;
        _thread = std::thread([&](){
            _is_running = true;
            this->_timer_lock.lock();
            if(this->_timer_lock.try_lock_for(std::chrono::milliseconds(this->_timeout)) == true){
                _handler_lock.lock();
                if(this->_cancel_handler != nullptr){
                    this->_cancel_handler();
                }
                _handler_lock.unlock();
            }else{
                this->_timer_lock.unlock();
                _handler_lock.lock();
                if(this->_timeout_handler != nullptr){
                    this->_timeout_handler();
                }
                _handler_lock.unlock();
            }
            _is_running = false;
        });
        _thread.detach();
    }

    void cancel(){
        _timer_lock.unlock();
    }
};

#endif
