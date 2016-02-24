// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache()
{
    init();
}

void
lock_server::init()
{
    lock_state_map.clear();
    VERIFY(pthread_mutex_init(&lock_mutex, 0) == 0);
    VERIFY(pthread_cond_init(&lock_cond, 0) == 0);
}


lock_protocol::status 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                           int &)
{
    lock_protocol::status ret = lock_protocol::OK;

    std::map<lock_protocol::lockid_t, server_lock_status>::iterator iter;
    iter = lock_state_map.find(lid);
    if (iter != lock_state_map.end()){
        if (iter->second.hold){
            //如果等待队列没有这个client, 放入队列
            bool flag = true;
            for (std::list<string>::iterator it=iter->second.clt_wait_list.begin(); 
                it != iter->second.clt_wait_list.end(); ++it){
                if (*it == id){
                    flag = false;
                    break;
                }
            }
            if (flag) {
                iter->second.clt_wait_list.push_back(id);
            }

            if (id == iter->second.clt_wait_list.front()) {
                //是否等待成功
                bool bwait_suc = false;
                //向holder发送revoke
                handle h(id);
                rlock_protocol::status cret;
                if (h.safebind()) {
                    cret = h.safebind()->call(rlock_protocol::revoke, lid);
                }
                if (h.safebind() && cret == rlock_protocol::OK) {
                    //退出等待队列
                    iter->second.clt_wait_list.pop_front();
                    bwait_suc = true;
                }
                //等待释放
                while (iter != lock_state_map.end() && iter->second.hold == true){
                    VERIFY(pthread_cond_wait(&lock_cond, &lock_mutex)==0);
                    printf("== waiting %d request from clt %d\n", (int)lid, clt);
                }

                //向下一个waiter发送retry
                while (!iter->second.clt_wait_list.empty()){
                    std::string dst = iter->second.clt_wait_list.front();
                    handle h1(dst);
                    if (h1.safebind()) {
                        cret = h1.safebind()->call(rlock_protocol::retry, lid);
                        if (!h1.safebind() || cret != rlock_protocol::OK) {
                          iter->second.clt_wait_list.pop_front();
                          continue;
                        }
                        break;
                    } else {
                        iter->second.clt_wait_list.pop_front();
                    }
                }

                //返回结果
                if (bwait_suc)
                    ret = lock_protocol::OK;
                else
                    ret = lock_protocol::RETRY;
                    goto release;
            } else {
                //进入等待队列, 并再请求一次
                ret = lock_protocol::RETRY;
                goto release;
            }
        } else {
            iter->second.hold = true;
            iter->second.id = id;
            iter->second.clt_wait_list.clear();
        }
    } else {
        server_lock_status status;
        status.hold = true;
        status.id = id;
        status.clt_wait_list.clear();
        lock_state_map[lid] = status;
    }

release:
    return ret;
}

lock_protocol::status 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
     int &r)
{
    lock_protocol::status ret = lock_protocol::OK;

    iter = lock_state_map.find(lid);
    if (iter != lock_state_map.end()) {
        if (iter->second.hold && iter->second.id == id) {
            iter->second.hold = false;
            VERIFY(pthread_cond_broadcast(&lock_cond) == 0);
            printf("<- release %d request from clt %s\n", (int)lid, id);
        }
    }

    return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, std::string id,
                    int &r)
{
    tprintf("stat request\n");
    r = nacquire;
    return lock_protocol::OK;
}

