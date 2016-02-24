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
}

lock_client_cache::~lock_client_cache()
{
  if(rlsrpc)
    delete rlsrpc;
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;

  do {
    std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
    iter = lock_status_map.find(lid);
    //状态判断
    if (iter != lock_status_map.end() && iter->second.cls == clock_status::LOCKED) {
      iter->second.ref++;
      ret = lock_protocol::OK;
      goto release;
    }

    //更新状态
    if (iter == lock_status_map.end()) {
      clock_status clstatus;
      clstatus.cls = clock_status::NONE;
      clstatus.ref = 0;
      lock_status_map[lid] = clstatus;
    } else {
      (iter != lock_status_map.end()) 
      iter->second.cls == clock_status::ACQUIRING;
      iter->second.ref = 0;
    }
    //修改状态
    ret = cl->call(lock_protocol::acquire, lid, id, r);
    if (ret == lock_protocol::OK) {
      iter->second.cls = clock_status::LOCKED;
      iter->second.ref = 1;
      //拿到走人
      goto release;
    }
  } while (ret == lock_protocol::RETRY);

 release:
  printf("client %s acquire and ret : %d\n", id.c_str(), ret);
  return ret;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;

  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if (iter == lock_status_map.end()) {
    ret = lock_protocol::OK;
    goto release;
  }

  if (iter->second.cls == clock_status::LOCKED) {
    if (iter->second.ref > 1) {
      iter->second.ref--;
      ret = lock_protocol::OK;
      goto release;     // 这里ref减到0, 也没有真正的去进行release, revoke的时候才release
    }
  }
  iter->second.cls == clock_status::RELEASING;
  ret = cl->call(lock_protocol::release, lid, id, r);
  if (ret == lock_protocol::OK) {
    iter->second.cls = clock_status::FREE;
    iter->second.ref = 0;
  }

 release:
  printf("client %s release and ret : %d\n", id.c_str(), ret);
  return ret;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  rlock_protocol::status ret = rlock_protocol::OK;

  std::map<lock_protocol::lockid_t, clock_status>::iterator iter;
  iter = lock_status_map.find(lid);
  if (iter != lock_status_map.end()) {

    if (iter->second.cls == clock_status::FREE) {
      ret = rlock_protocol::OK;
      goto release;
    }
    ret = cl->call(lock_protocol::release, lid, id, r);
    if (ret == lock_protocol::OK) {
      iter->second.cls = clock_status::FREE;
      iter->second.ref = 0;
    }
  }

 release:
  return ret;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  rlock_protocol::status ret = rlock_protocol::OK;
  return ret;
}



