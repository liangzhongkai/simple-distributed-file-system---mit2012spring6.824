#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"


class lock_server_cache {
 private:
    //lab 4
    struct server_lock_status{
        bool hold; //是否被持有
        string id;  //ip:port   标记client
        std::list<string> clt_wait_list;
    };
 private:
  int nacquire;
  //lab 4
  pthread_mutex_t                                      lock_mutex;
  pthread_cond_t                                       lock_cond;
  std::map<lock_protocol::lockid_t, server_lock_status> lock_state_map;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, std::string id, int &);
  lock_protocol::status acquire(lock_protocol::lockid_t, std::string id, int &);
  lock_protocol::status release(lock_protocol::lockid_t, std::string id, int &);
  //lab 4
  void init();
};

#endif
