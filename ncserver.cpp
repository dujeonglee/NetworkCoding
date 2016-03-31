#include "ncserver.h"
#include "networkcodingheader.h"
#include "finite_field.h"
#include <exception>
#include <string.h>

ncserver::ncserver(unsigned int client_ip, unsigned short int port, unsigned char max_block_size, unsigned int timeout):\
                                    _CTRL_ADDR(build_addr(INADDR_ANY, port)), _DATA_ADDR(build_addr(client_ip, port)), \
                                    _MAX_BLOCK_SIZE(max_block_size), _TX_TIMEOUT(timeout){
    _socket = 0;
    _tx_cnt = 0;
    _blk_seq = 0;
    _pkt_seq = 0;
    _redundant_pkts = 2;

	/*
	 * TODO: 1) Encode meaningful pkts in _buffer. (_buffer may not be full when _flush_pkts is triggered by timeout)
	 *       2) The length of remedy packet should be as most that of the longest pkt in _buffer.
	 *       3) Parallelize the encoding process using OpenMP.
	 * NOTE: 1) This function should be called by singleshottimer. Otherwise, its operation is undefied.
	 */
	_cancel_callback = _timeout_callback = [&](){
		std::lock_guard<std::mutex> lock(_lock);

		int ret;
		for(unsigned int tx = 0 ; tx < _redundant_pkts ; tx++){
			for(unsigned int code = 0 ; code < _MAX_BLOCK_SIZE ; code++){
				_rand_coef[code] = 1+rand()%255;
			}
			GET_BLK_SEQ(_remedy_pkt) = _blk_seq;
			GET_PKT_SEQ(_remedy_pkt) = _pkt_seq++;
			GET_BLK_SIZE(_remedy_pkt) = _MAX_BLOCK_SIZE;
			for(unsigned int position = OUTERHEADER_SIZE ; position < HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE) ; position++){
				for(unsigned int pkt = 0 ; pkt < _MAX_BLOCK_SIZE ; pkt++){
					(pkt == 0?
						_remedy_pkt[pos] = FiniteField::instance()->mul(_buffer[pkt][position], _rand_coef[pkt]):
						_remedy_pkt[pos] ^= FiniteField::instance()->mul(_buffer[pkt][position], _rand_coef[pkt]));
				}
			}
			ret = sendto(_socket, _remedy_pkt, HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR));
			if(ret == -1){
				std::cout << "Cannot send remedy pkt" << std::endl;
			}
		}
		_blk_seq++;
		_tx_cnt = 0;
	};
}

ncserver::~ncserver(){
    close_server();
}

sockaddr_in ncserver::build_addr(unsigned int ip, unsigned short int port){
    sockaddr_in ret = {0};
    ret.sin_family = AF_INET;
    ret.sin_addr.s_addr = htonl(ip);
    ret.sin_port = htons(port);
    return ret;
}

bool ncserver::open_server(){
    _socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(_socket == -1){
        return false;
    }
    int opt = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1){
        close(_socket);
        return false;
    }
    if(bind(_socket, (sockaddr*)&_CTRL_ADDR, sizeof(_CTRL_ADDR)) == -1){
        close(_socket);
        return false;
    }
    try{
        _buffer = new unsigned char*[_MAX_BLOCK_SIZE];
		for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
			_buffer[i] = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
		}
		_remedy_pkt = new unsigned char[HEADER_SIZE(_MAX_BLOCK_SIZE)+MAX_PAYLOAD_SIZE(_MAX_BLOCK_SIZE)];
		_rand_coef = new unsigned char[_MAX_BLOCK_SIZE];
    }catch(std::exception &ex){
        return false;
    }
    return true;
}

/*
 * TODO: Check if this function can be further optimized using rvalue reference using std::move()
 */
unsigned short int ncserver::send(unsigned char* pkt, unsigned short int pkt_size){
	std::lock_guard<std::mutex> lock(_lock);
	unsigned short int ret = 0;
	if(pkt_size > MAX_PAYLOAD_SIZE){
		return 0; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.
	}

	GET_BLK_SEQ(_buffer[_tx_cnt]) = _blk_seq;

	GET_PKT_SEQ(_buffer[_tx_cnt]) = _pkt_seq++;

	GET_BLK_SIZE(_buffer[_tx_cnt]) = _MAX_BLOCK_SIZE;

	GET_SIZE(_buffer[_tx_cnt]) = pkt_size;

	GET_LAST_INDICATOR(_buffer[_tx_cnt]) = 1; // I will support pkt_size larger than MAX_PAYLOAD_SIZE in the next phase.

	memset(GET_CODE(_buffer[_tx_cnt]), 0x0, _MAX_BLOCK_SIZE);
	GET_CODE(_buffer[_tx_cnt])[_tx_cnt] = 1;

	memcpy(GET_PAYLOAD(_buffer[_tx_cnt], _MAX_BLOCK_SIZE), pkt, pkt_size);

	if(sendto(_socket, _buffer[_tx_cnt], pkt_size + HEADER_SIZE(_MAX_BLOCK_SIZE), 0, (sockaddr*)&_DATA_ADDR, sizeof(_DATA_ADDR)) == pkt_size + HEADER_SIZE(_MAX_BLOCK_SIZE)){
		ret = pkt_size;
	}else{
		ret = 0;
	}
	if(_tx_cnt == 0){
		_timer.start(_TX_TIMEOUT, _cancel_callback, _timeout_callback);
	}else if(_tx_cnt == _MAX_BLOCK_SIZE-1){
		_timer.cancel();
	}
	_tx_cnt++;
	return ret;
}

void ncserver::close_server(){
	delete [] _rand_coef;
	delete [] _remedy_pkt;
    for(unsigned int i = 0 ; i < _MAX_BLOCK_SIZE ; i++){
        delete [] _buffer[i];
    }
    delete []_buffer;

    if(_socket){
        close(_socket);
    }
}
