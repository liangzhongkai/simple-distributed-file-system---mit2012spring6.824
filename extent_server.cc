// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server() 
{
  pthread_mutex_init(&mutex, NULL);
  file_map.clear();
  int ret;
  put(1, "", ret);//初始化根目录
}


int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{
  // You fill this in for Lab 2.
  printf("extent_server::put\n");
  ScopedLock _l(&mutex);
  extent_protocol::attr attr;
  attr.atime = attr.mtime = attr.ctime = time(NULL);
  if (file_map.find(id) != file_map.end()){
    attr.atime = file_map[id].attr.atime;
  }
  attr.size = buf.size();
  file_map[id].data = buf;
  file_map[id].attr = attr; 
  return extent_protocol::OK;
}

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  // You fill this in for Lab 2.
  printf("extent_server::get\n");
  ScopedLock _l(&mutex);
  if (file_map.find(id) != file_map.end()){
    buf = file_map[id].data;
    file_map[id].attr.atime = time(NULL);
  } else {
    buf = "";
    printf("extent_server::get no file\n");
    return extent_protocol::NOENT;
  }
  return extent_protocol::OK;
}

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You fill this in for Lab 2.
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  printf("extent_server::getattr\n");
  a.size = 0;
  a.atime = 0;
  a.mtime = 0;
  a.ctime = 0;
  ScopedLock _l(&mutex);
  std::map<extent_protocol::extentid_t, inode>::iterator iter;
  iter = file_map.find(id);
  if (iter != file_map.end()){
    iter->second.attr.atime = time(NULL);
    a = iter->second.attr;
  } else {
    printf("extent_server::getattr no file\n");
    return extent_protocol::NOENT;
  }
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{
  // You fill this in for Lab 2.
  printf("extent_server::remove\n");
  ScopedLock _l(&mutex);
  std::map<extent_protocol::extentid_t, inode>::iterator iter;
  iter = file_map.find(id);
  if (iter != file_map.end()){
    file_map.erase(iter);
  } else {
    printf("extent_server::remove no file\n");
    return extent_protocol::NOENT;
  }
  return extent_protocol::OK;
}

