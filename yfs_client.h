#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>

#include "lock_protocol.h"
#include "lock_client.h"

class yfs_client {
  extent_client *ec;
  lock_client *lc; 
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  static std::string filename(inum);
  static inum n2i(std::string);
 public:

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  //lab2
  inum random_inum(bool);
  int create(inum parent, const char *name, inum &inum, bool isfile=true);
  int getfile(inum inum, std::string &fstr);
  int getdir(inum inum, std::list<dirent> &dirents);
  int lookup(inum parent, const char *name, inum &inum, bool *found);
  int setattr(inum inum, struct stat *attr);
  int read(inum inum, off_t off, size_t size, std::string &buf);
  int write(inum inum, off_t off, size_t size, const char *buf);

  //lab3
  int unlink(inum parent, const char *name);
};

class ScopedLockClient{
  public:
    ScopedLockClient(lock_client *lc, lock_protocol::lockid_t lid):
      lc_(lc),lid_(lid) {
        lc_->acquire(lid_);
      }
    ~ScopedLockClient() {
      lc_->release(lid_);
    }
  private:
    lock_client *lc_;
    lock_protocol::lockid_t lid_;
};

#endif 
