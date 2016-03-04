#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <set>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

static int                                            server_acquire_count = 0;

class lock_server_cache {
 private:
    //lab 4
    //FREE: 完全没有client占有
    //LOCKED: 有一个client占有
    //WAITING: 有client开始等
    //RETRYING: 将控制权交给等待队列中的一个client
    enum server_lock_status { FREE, LOCKED, WAITING, RETRYING };
    struct slock_status {
      server_lock_status    slstatus;
      std::string           id;
      std::set<std::string> wait_clients;
      slock_status() : slstatus(FREE), id("") {}
    };
 private:
  int nacquire;
  //lab 4
  pthread_mutex_t                                       lock_mutex;//保护lock status的变化
  std::map<lock_protocol::lockid_t, slock_status>       lock_state_map;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, std::string id, int &r);
  lock_protocol::status acquire(lock_protocol::lockid_t, std::string id, int &r);
  lock_protocol::status release(lock_protocol::lockid_t, std::string id, int &r);
  //lab 4
  void init();
};

#endif
