// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

class lock_server {

 protected:
  int nacquire;

 public:
  lock_server();
  ~lock_server() {};
  void init();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
 private:
  //pthread_mutex_t create_lock;
  //std::map<lock_protocol::lockid_t, pthread_mutex_t> lock_mutex_map;
  //std::map<lock_protocol::lockid_t, pthread_cond_t> lock_cond_map;
  std::map<lock_protocol::lockid_t, int> lock_clt_map;
  pthread_mutex_t                        lock_mutex;
  pthread_cond_t                         lock_cond;
};

#endif 







