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
	pthread_cond_init(&cond, NULL);
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
	std::map<lock_protocol::lockid_t, int>::iterator iter;
	iter = locks.find(lid);
	if(iter == locks.end()){
		//not found
		locks.insert(std::pair<lock_protocol::lockid_t, int>(lid, LOCKED));
	}else if(iter->second == LOCKFREE){
		//found and not locked
		iter->second = LOCKED;
	}else{
		//found and locked
		while(iter->second == LOCKED){
			pthread_cond_wait(&cond, &mutex);
			//pthread_mutex_unlock(&mutex);
			//sleep(1);
			//pthread_mutex_lock(&mutex);
		}
		iter->second = LOCKED;
	}
	r = lid;
	nacquire++;
	printf("acquired lock:%lld from:%d nacquire=%d\n", lid, clt, nacquire);
	pthread_mutex_unlock(&mutex);
	return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
	pthread_mutex_lock(&mutex);
	std::map<lock_protocol::lockid_t, int>::iterator iter;
	iter = locks.find(lid);
	if(iter == locks.end()){
		//not found
		ret = lock_protocol::NOENT;
	}else if(iter->second == LOCKFREE){
		//found but not locked
		ret = lock_protocol::RETRY;
	}else{
		//locked
		iter->second = LOCKFREE;
		nacquire--;
	}
	r = lid;
	printf("release lock:%lld from:%d nacquire=%d\n", lid, clt, nacquire);
	pthread_mutex_unlock(&mutex);
	pthread_cond_signal(&cond);
	return ret;
}
