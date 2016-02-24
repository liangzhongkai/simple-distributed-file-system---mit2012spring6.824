// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  init();
}

void
lock_server::init()
{
  lock_clt_map.clear();
  lock_clt_map.insert(std::map<lock_protocol::lockid_t, int>::value_type( 0, 0 ));
  VERIFY(pthread_mutex_init(&lock_mutex, 0) == 0);
  VERIFY(pthread_cond_init(&lock_cond, 0) == 0);
}


lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);

  {
    ScopedLock ml(&lock_mutex);
    std::map<lock_protocol::lockid_t, int>::iterator iter;
    iter = lock_clt_map.find(lid);
    if( iter != lock_clt_map.end() ){
      r = iter->second;
    } else {
      ret = lock_protocol::NOENT;
      r = nacquire;
    }
  }

  return ret;
}

lock_protocol::status 
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  
  {
    ScopedLock ml(&lock_mutex);
    while( lock_clt_map.find(lid) != lock_clt_map.end() ){
      VERIFY(pthread_cond_wait(&lock_cond, &lock_mutex)==0);
      printf("== waiting %d request from clt %d\n", (int)lid, clt);
    }
    lock_clt_map[lid] = clt;
    r = clt;
    printf("-> acquire %d request from clt %d\n", (int)lid, clt);
  }

  return ret;
}

lock_protocol::status 
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  {
    ScopedLock ml(&lock_mutex);
    std::map<lock_protocol::lockid_t, int>::iterator iter;
    iter = lock_clt_map.find(lid);
    if( iter != lock_clt_map.end() ){
      if( clt == iter->second ){
        r = iter->second;
        lock_clt_map.erase(iter);
        VERIFY(pthread_cond_broadcast(&lock_cond) == 0);
        printf("<- release %d request from clt %d\n", (int)lid, clt);
      }
    }
  }
  
  return ret;
}

