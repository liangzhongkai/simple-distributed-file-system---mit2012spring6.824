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
lock_server_cache::init()
{
    lock_state_map.clear();
    VERIFY(pthread_mutex_init(&lock_mutex, 0) == 0);
}


lock_protocol::status 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, 
                           int &r)
{
    bool revoke = false;
    std::string revoke_client;
    lock_protocol::status ret = lock_protocol::OK;

    std::map<lock_protocol::lockid_t, slock_status>::iterator iter;

    {
        ScopedLock ml(&lock_mutex);
        server_acquire_count++;
        tprintf("%d id %s  lid %d\n", server_acquire_count, id.c_str(), (int)lid);
        iter = lock_state_map.find(lid);
        if (iter == lock_state_map.end()) {
            iter = lock_state_map.insert(std::make_pair(lid, slock_status())).first;
            VERIFY (iter != lock_state_map.end());
        }

        switch (iter->second.slstatus) {
            case FREE: {
                    iter->second.slstatus = LOCKED;
                    iter->second.id = id;
                    tprintf("    acquire lid %d  id %s\n", (int)lid, id.c_str());
                }
                break;
            case LOCKED: {
                    if (id != iter->second.id) {
                        iter->second.slstatus = WAITING;
                        revoke = true;
                        revoke_client = iter->second.id;
                        iter->second.wait_clients.insert(id);//唯一性
                        ret = lock_protocol::RETRY;
                        tprintf("    acquire begin wait lid %d  id %s, owner %s, size %d\n", (int)lid, id.c_str(), iter->second.id.c_str(), (int)iter->second.wait_clients.size());
                    }
                }
                break;
            case WAITING: {
                    if (id != iter->second.id) {
                        iter->second.wait_clients.insert(id);//唯一性
                        ret = lock_protocol::RETRY;
                        tprintf("    acquire wait queue lid %d  id %s, size %d\n", (int)lid, id.c_str(), (int)iter->second.wait_clients.size());
                    }
                }
                break;
            case RETRYING: {
                    if (iter->second.wait_clients.count(id)) {
                        iter->second.wait_clients.erase(id);
                        iter->second.id = id;
                        iter->second.slstatus = LOCKED;
                        if (iter->second.wait_clients.size()) {
                            iter->second.slstatus = WAITING;
                            revoke = true;
                            revoke_client = id;
                            tprintf("    acquire 1 lid %d  id %s, size %d\n", (int)lid, id.c_str(), (int)iter->second.wait_clients.size());
                        } else {
                            tprintf("    acquire 2 lid %d  id %s\n", (int)lid, id.c_str());
                        }
                    } else {
                        iter->second.wait_clients.insert(id);
                        ret = lock_protocol::RETRY;
                        tprintf("    acquire wait queue lid %d  id %s, size %d\n", (int)lid, id.c_str(), (int)iter->second.wait_clients.size());
                    }
                }
                break;
            default:
                break;
        }
    }
    
    if (revoke) {
        int cnt = 0;
        rlock_protocol::status ret = 1;
        while (ret != 0 && cnt < 7) {
            cnt++;
            ret = handle(revoke_client).safebind()->call(rlock_protocol::revoke, lid, r);
            tprintf("%s revoke %s ret : %d\n", id.c_str(), revoke_client.c_str(), ret);
        }
    }

    return ret;
}

lock_protocol::status 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, 
     int &r)
{
    bool retry = false;
    std::string retry_client;
    lock_protocol::status ret = lock_protocol::OK;

    std::map<lock_protocol::lockid_t, slock_status>::iterator iter;

    {
        ScopedLock ml(&lock_mutex);
        server_acquire_count++;
        tprintf("%d id %s  lid %d\n", server_acquire_count, id.c_str(), (int)lid);
        iter = lock_state_map.find(lid);
        if (iter == lock_state_map.end()) {
            iter = lock_state_map.insert(std::make_pair(lid, slock_status())).first;
            VERIFY (iter != lock_state_map.end());
        }

        switch (iter->second.slstatus) {
            case FREE: {
                    tprintf("ERROR: id %s release a lid %d when it's state is FREE\n", id.c_str(), (int)lid);//这个说明client的状态跟server的不同步
                    ret = lock_protocol::IOERR;
                }
                break;
            case LOCKED: {
                    if (id == iter->second.id) {
                        iter->second.slstatus = FREE;
                        iter->second.id = "";
                        tprintf("    release lid %d  id %s\n", (int)lid, id.c_str());
                    }
                }
                break;
            case WAITING: {
                    if (id == iter->second.id) {
                        iter->second.slstatus = RETRYING;
                        iter->second.id = "";
                        retry = true;
                        retry_client = *iter->second.wait_clients.begin();
                        tprintf("    release retry lid %d  id %s retry id %s , size %d\n", (int)lid, id.c_str(), retry_client.c_str(), (int)iter->second.wait_clients.size());
                    }
                }
                break;
            case RETRYING: {
                    tprintf("ERROR: id %s release a lid %d when it's state is RETRYING\n", id.c_str(), (int)lid);
                    ret = lock_protocol::IOERR;
                }
                break;
            default:
                break;
        }
    }

    if (retry) {
        int cnt = 0;
        rlock_protocol::status ret = 1;
        while (ret != 0 && cnt < 7) {
            cnt++;
            ret = handle(retry_client).safebind()->call(rlock_protocol::retry, lid, r);
            tprintf("%s retry %s ret : %d\n", id.c_str(), retry_client.c_str(), ret);
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

