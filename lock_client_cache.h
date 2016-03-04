// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 5.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  //NONE：当前lock_client_cache对lock没有控制权
  //FREE：当前lock_client_cache没有线程占有, 但是保留控制权
  //LOCKED：当前lock_client_cache的某个线程已占有<已有控制权>
  //ACQUIRING：当前lock_client_cache的某个线程发出acquire lock的命令,准备获得控制权
  //RELEASING：当前lock_client_cache的某个线程发出release lock的命令,准备交出控制权
  enum client_lock_status { NONE, FREE, LOCKED, ACQUIRING, RELEASING };
  struct clock_status{
    bool flag_revoke;//revoke之后如果有线程占有，则等待线程释放之后才交出控制权
    //只允许cache的一个线程向server发出获取控制权的命令
    //因为call(acquire)的返回可能比接收到retry消息慢，可能会出现先retry了，需要acquire的线程才进入retry_queue
    //    这样的话，retry就相当于失效了!!!  必须要先进入retry_queue才能触发cond_retry
    //    折中办法：在没有接收到retry之前可以进入队列<之后可以有retry唤醒>，但是接收后就不要进入，直接继续下次请求<这里会引发多次请求>
    bool flag_retry; 
    client_lock_status clstatus;
    //三种状态时的acquire行为的控制
    pthread_cond_t     cond_acquire;
    pthread_cond_t     cond_release;
    pthread_cond_t     cond_retry;
    clock_status() : flag_revoke(false), flag_retry(false), clstatus(NONE) {
      VERIFY(pthread_cond_init(&cond_acquire, NULL) == 0);
      VERIFY(pthread_cond_init(&cond_release, NULL) == 0);
      VERIFY(pthread_cond_init(&cond_retry, NULL) == 0);
    }
  };
  
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;   //ip:port
  //lab 4
  rpcs *rlsrpc;
  std::map<lock_protocol::lockid_t, clock_status> lock_status_map;//维护一小部分lock, 这里影响了call操作
  pthread_mutex_t                                 lock_mutex;//保护lock状态的修改
 public:
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache();
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, 
                                        int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, 
                                       int &);

};


#endif
