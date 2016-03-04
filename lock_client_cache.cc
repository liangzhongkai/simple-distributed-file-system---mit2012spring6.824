// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"


lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  rlsrpc = new rpcs(0);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);

  const char *hname;
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlsrpc->port();
  id = host.str();

  lock_status_map.clear();
  VERIFY(pthread_mutex_init(&lock_mutex, NULL) == 0);
}

lock_client_cache::~lock_client_cache()
{
  if(rlsrpc)
    delete rlsrpc;

  pthread_mutex_lock(&lock_mutex);
  int r;
  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  for (iter=lock_status_map.begin(); iter!=lock_status_map.end(); ++iter) {
    if (iter->second.clstatus != NONE && iter->second.clstatus != ACQUIRING && iter->second.clstatus != RELEASING){
      tprintf("<-- end release client %s  lockid %d, return control\n", id.c_str(), (int)iter->first);
      cl->call(lock_protocol::release, iter->first, id, r);
    }
  }
  pthread_mutex_unlock(&lock_mutex);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  tprintf("-> begin acquire client %s  lockid %d\n", id.c_str(), (int)lid);
  lock_protocol::status ret = lock_protocol::OK;
  int r;

  pthread_mutex_lock(&lock_mutex);

  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if(iter == lock_status_map.end()) {
    iter = lock_status_map.insert(std::make_pair(lid, clock_status())).first;
    VERIFY (iter != lock_status_map.end());
  }

  //整个循环的目的就是为了让当前操作这个lock_client_cache的线程亲自设置lock为LOCKED状态
  while (true) {
    //下面所有的阻塞都是为了重新跑到NONE状态去获取控制
    switch (iter->second.clstatus) {
      case NONE: {
          tprintf("-> status NONE acquire client %s  lockid %d\n", id.c_str(), (int)lid);
          iter->second.clstatus = ACQUIRING;
          iter->second.flag_retry = true;
          pthread_mutex_unlock(&lock_mutex);
          
          ret = cl->call(lock_protocol::acquire, lid, id, r);
          pthread_mutex_lock(&lock_mutex);
          if (ret == lock_protocol::OK) {
            iter->second.clstatus = LOCKED;
            pthread_mutex_unlock(&lock_mutex);
            tprintf("--> end acquire client %s  lockid %d\n", id.c_str(), (int)lid);
            return ret;
          } else if (ret == lock_protocol::RETRY) {
            if (iter->second.flag_retry){
              tprintf("-> status NONE wait in retry queue client %s  lockid %d\n", id.c_str(), (int)lid);
              pthread_cond_wait(&iter->second.cond_retry, &lock_mutex);
            }
          }
        }
        break;
      case FREE: {//还保留控制权限
          tprintf("-> status FREE acquire client %s  lockid %d\n", id.c_str(), (int)lid);
          iter->second.clstatus = LOCKED;
          pthread_mutex_unlock(&lock_mutex);
          return lock_protocol::OK;
        }
        break;
      case RELEASING: { //退换控制权中，这里之后状态只能是NONE
          tprintf("-> status RELEASING acquire client %s  lockid %d\n", id.c_str(), (int)lid);
          pthread_cond_wait(&iter->second.cond_release, &lock_mutex);
        }
        break;
      case LOCKED: {//这里是让lock_client_cache的非holder线程阻塞
          tprintf("-> status LOCKED acquire client %s  lockid %d\n", id.c_str(), (int)lid);
          pthread_cond_wait(&iter->second.cond_acquire, &lock_mutex);
        }
        break;
      case ACQUIRING: {
          tprintf("-> status ACQUIRING acquire client %s  lockid %d\n", id.c_str(), (int)lid);
          //if (!iter->second.flag_retry) {
          //  tprintf("-> status ACQUIRING wait in acquire queue client %s  lockid %d\n", id.c_str(), lid);
          //  pthread_cond_wait(&iter->second.cond_retry, &lock_mutex);
          //} else {
            //iter->second.flag_retry = true;
            pthread_mutex_unlock(&lock_mutex);

            ret = cl->call(lock_protocol::acquire, lid, id, r);
            pthread_mutex_lock(&lock_mutex);
            if (ret == lock_protocol::OK) {
              iter->second.clstatus = LOCKED;
              pthread_mutex_unlock(&lock_mutex);
              tprintf("--> end acquire client %s  lockid %d\n", id.c_str(), (int)lid);
              return ret;
            } else if (ret == lock_protocol::RETRY) {
              if (iter->second.flag_retry) { //拦截获取控制权的命令，一次不需要一个以上的请求
                tprintf("-> status ACQUIRING wait in retry queue client %s  lockid %d\n", id.c_str(), (int)lid);
                pthread_cond_wait(&iter->second.cond_retry, &lock_mutex);
              }
            }
          //}
        }
        break;
    }
    tprintf("-> next loop begin acquire client %s  lockid %d\n", id.c_str(), (int)lid);
  }
  
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  tprintf("<- begin release client %s  lockid %d\n", id.c_str(), (int)lid);
  lock_protocol::status ret = lock_protocol::OK;
  int r;

  pthread_mutex_lock(&lock_mutex);

  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if(iter == lock_status_map.end()) {
    tprintf("release error, no lock %d in this cache\n", (int)lid);
    pthread_mutex_unlock(&lock_mutex);
    return lock_protocol::NOENT;  //没有这个锁
  }

  if (iter->second.flag_revoke) {
    iter->second.clstatus = RELEASING;
    iter->second.flag_revoke = false;
    pthread_mutex_unlock(&lock_mutex);

    ret = cl->call(lock_protocol::release, lid, id, r);

    pthread_mutex_lock(&lock_mutex);
    iter->second.clstatus = NONE;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_broadcast(&iter->second.cond_release);
    tprintf("<-- end release client %s  lockid %d, return control\n", id.c_str(), (int)lid);
  } else {
    tprintf("<-- end release client %s  lockid %d, but no return control\n", id.c_str(), (int)lid);
    iter->second.clstatus = FREE;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_broadcast(&iter->second.cond_acquire);
  }

  
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  tprintf("=> begin revoke client %s  lockid %d\n", id.c_str(), (int)lid);
  rlock_protocol::status ret = rlock_protocol::OK;
  int r;

  pthread_mutex_lock(&lock_mutex);

  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if(iter == lock_status_map.end()) {
    tprintf("revoke error, no lock %d in this cache\n", (int)lid);
    pthread_mutex_unlock(&lock_mutex);
    return lock_protocol::NOENT;  //没有这个锁
  }

  if (iter->second.clstatus == FREE) {//本地lock_client_cache没有线程占有
    iter->second.clstatus = RELEASING;
    pthread_mutex_unlock(&lock_mutex);

    ret = cl->call(lock_protocol::release, lid, id, r);

    pthread_mutex_lock(&lock_mutex);
    iter->second.clstatus = NONE;
    pthread_mutex_unlock(&lock_mutex);

    pthread_cond_broadcast(&iter->second.cond_release);
    
  } else {
    iter->second.flag_revoke = true;   //这个标记是为使lock的状态转为NONE
    pthread_mutex_unlock(&lock_mutex);
  }

  tprintf("==> end revoke client %s  lockid %d\n", id.c_str(), (int)lid);
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  tprintf("<= begin retry client %s  lockid %d\n", id.c_str(), (int)lid);
  rlock_protocol::status ret = rlock_protocol::OK;

  pthread_mutex_lock(&lock_mutex);
  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if(iter == lock_status_map.end()) {
    tprintf("retry error, no lock %d in this cache\n", (int)lid);
    pthread_mutex_unlock(&lock_mutex);
    return lock_protocol::NOENT;  //没有这个锁
  }
  iter->second.flag_retry = false;
  pthread_mutex_unlock(&lock_mutex);

  pthread_cond_broadcast(&iter->second.cond_retry);//打开栏栅，可以再次请求

  tprintf("<== end retry client %s  lockid %d\n", id.c_str(), (int)lid);
  return ret;
}



