// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	pthread_mutex_init(&mutex, NULL);
}


lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, long long >::iterator iter;
	iter = locks.find(lid);

	gettimeofday(&t_val, NULL);
	long long time_l = (long long)t_val.tv_sec*MILLISEC+t_val.tv_usec;
	if(iter == locks.end()){
		//not found
		locks.insert(std::pair<lock_protocol::lockid_t, long long>(lid, time_l));
	}else if(iter->second  + TIMEOUT <= time_l){	//timeout
		//found and not locked	
		iter->second = (long long)t_val.tv_sec*MILLISEC+t_val.tv_usec;
	}else{
		//found and locked
		while(iter->second + TIMEOUT > time_l){	//locked
			pthread_mutex_unlock(&mutex);
			usleep(TIMEWAIT);
			pthread_mutex_lock(&mutex);
		}
		gettimeofday(&t_val, NULL);
		iter->second = (long long)t_val.tv_sec*MILLISEC+t_val.tv_usec;
	}
	r = lid;
	nacquire++;
//	printf("acquired lock:%lld from:%d nacquire=%d\n", lid, clt, nacquire);
	pthread_mutex_unlock(&mutex);
	return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, long long>::iterator iter;
	iter = locks.find(lid);
	if(iter == locks.end()){
		//not found
		ret = lock_protocol::NOENT;
	}else if(iter->second == 0){
		//found but not locked
		ret = lock_protocol::RETRY;
	}else{
		//locked
		iter->second = 0;
		nacquire--;
	}
	r = lid;
//	printf("release lock:%lld from:%d nacquire=%d\n", lid, clt, nacquire);
	pthread_mutex_unlock(&mutex);
	return ret;
}
