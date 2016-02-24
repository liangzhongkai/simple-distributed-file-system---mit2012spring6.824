// lock protocol

#ifndef lock_protocol_h
#define lock_protocol_h

#include "rpc.h"

class lock_protocol {
 public:
  //返回状态
  // RETRY: try again
  // RPCERR: rpc error
  // NOENT: no such file or directory
  // IOERR: io error
  enum xxstatus { OK, RETRY, RPCERR, NOENT, IOERR };   
  typedef int status;
  //锁的ID
  typedef unsigned long long lockid_t;
  typedef unsigned long long xid_t;
  //rpc的ID
  enum rpc_numbers {
    acquire = 0x7001,
    release,
    stat
  };
};

class rlock_protocol {
 public:
  //RPC返回状态
  enum xxstatus { OK, RPCERR };
  typedef int status;
  //RPC请求状态
  enum rpc_numbers {
    revoke = 0x8001,
    retry = 0x8002
  };
};
#endif 
