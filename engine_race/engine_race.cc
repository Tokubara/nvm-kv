// Copyright [2018] Alibaba Cloud All rights reserved
#include "engine_race.h"
#include "util.h"
#include <fcntl.h>
#include <iostream>
#include <map>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
namespace polar_race {

// #define which_bucket(ch) ((ch<='9'?ch-'0':(ch<='Z'?ch-'A'+10: (ch-'a'+36)))/num_per_bucket)
// #define which_bucket(ch) ((u8)ch)/num_per_bucket

const u8 num_per_bucket = (256+BUCKET_NUM-1)/BUCKET_NUM;

u8 which_bucket(char ch) {
  u8 ret = ((u8)ch)/num_per_bucket;
  Assert(ret<BUCKET_NUM,  "ch=%u,num_per_bucket=%d,ret=%u",(u32)ch,num_per_bucket, (u32)ret);
  return ret;
}

RetCode Engine::Open(const std::string& name, Engine** eptr) {
  return EngineRace::Open(name, eptr);
}

Engine::~Engine(){}

RetCode EngineRace::Open(const std::string &name, Engine **eptr) {
  log_trace("Open %s, bucket number: %d", name.c_str(), BUCKET_NUM);
  *eptr = NULL;
  EngineRace *engine = new EngineRace();
  engine->dir_name = name;
  std::string buf = "mkdir -p " + name;
  if (system(buf.c_str()) != 0) {
    return kIOError;
  }
  // {{{2 会被用于设置字段的变量
  u32 data_offset = 0;
  void *data = nullptr;
  u8 *cur = nullptr;
  i32 data_fd = -1, key_fd = -1;

  struct stat st; // 也是循环中临时变量的性质
  st.st_size = 0; // 没什么用, 只是为了编译警告

  std::string data_file_name;
  std::string index_file_name; 
  // init every bucket
  for (size_t i = 0; i < BUCKET_NUM; i++) {
    u32 key_count = 0;
    std::string suffix = std::to_string(i);
    index_file_name = name + "/index_" + suffix;
    data_file_name = name + "/data_" + suffix;
    // int fd_index;
    Bucket &f = engine->buckets[i];
    key_fd = open(index_file_name.c_str(), O_RDWR, 0666);
    // {{{2 目录已存在
    if (key_fd > 0) {
  // log_trace("engine exists");
      // 维护各个字段
      // 再打开数据文件
      data_fd = open(data_file_name.c_str(), O_RDWR, 0666);
      if (data_fd < 0) {
        log_error("cannot open data file, path=%s, id=%d", name.c_str(), i);
        perror("open data file fail");
        return kIOError;
      }
      fstat(data_fd, &st);
      data_offset = st.st_size;

      // mmap
      // fstat(key_fd, &st);
      // Assert(st.st_size == key_file_size, "st.st_size: %lu,key_file_size: %lu, key_fd=%d", st.st_size, key_file_size, key_fd);
      data = mmap(nullptr, key_file_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        key_fd, 0);
      if (data == MAP_FAILED) {
        perror("mmap fail");
        return kIOError;
      }
      // 解析index文件
      cur = (u8*)data; // 表示已经读取的index文件的位置
      while (true) {
        u32 key_sz = *(u32 *)cur;

        if (key_sz == 0) {
          // log_trace("index file finish, key_count:%u", key_count);
          break;
        } else {
          f.map.insert_or_assign(std::string((char*)cur + 4, key_sz),
                                 (Location *)(cur + 4 + key_sz));
          cur = cur + 4 + key_sz + location_sz;
          key_count++; // DEBUG
        }
      }

    } else {                                         // {{{2 目录不存在
      // 新目录, 创建data和index文件
      key_fd = open((name + "/index_" + suffix).c_str(), O_RDWR | O_CREAT, 0666);
      if (key_fd < 0) {
        perror("cannot create file");
        return kIOError;
      }
      data_fd = open((name + "/data_" + suffix).c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
      if (data_fd < 0) {
        perror("cannot open file");
        return kIOError;
      }
      data_offset = 0;
      if (posix_fallocate(key_fd, 0, key_file_size) != 0) {
        log_debug("key_fd:%d,name:%s,suffix:%s", key_fd, name.c_str(), suffix.c_str());
        perror("cannot posix_fallocate");
        return kIOError;
      }
      // 创建index的映射数组, 初始化与映射相关的各个字段
      data = mmap(NULL, key_file_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        key_fd, 0);
      if (data == MAP_FAILED) {
        perror("mmap fail");
        return kIOError;
      }
      cur = (u8*)data;
      memset(cur, 0, key_file_size);
    }
    // {{{2 公共字段
    // 维护各个字段(好像也就4个字段)
    f.key_mmap = (u8*)data;
    f.key_mmap_cur = cur;
    assert((u64)f.key_mmap_cur!=0);
    f.key_fd = key_fd;
    f.data_fd = data_fd;
    f.data_offset = data_offset;
    // 初始化锁
    // pthread_rwlock_init(&f.lock, nullptr);
    f.lock = PTHREAD_MUTEX_INITIALIZER;
  }
  *eptr = engine;
  return kSucc;
}

// {{{1 析构
EngineRace::~EngineRace() {
  // log_trace("Saving index");
  for (size_t i = 0; i < BUCKET_NUM; i++) {
    Bucket &f = buckets[i];
    munmap(f.key_mmap, key_file_size); // 因为的确用不着了, 信息都在index中
    close(f.key_fd);
    close(f.data_fd);
    // pthread_rwlock_destroy(&f.lock);
    pthread_mutex_destroy(&f.lock);
  }
}

// {{{1 write
// 需要维护的是2部分: index数组, 还有写文件, 不需要直接write, 是通过mmap的方式,
// memcpy就行了
RetCode EngineRace::Write(const PolarString &key, const PolarString &value) {
  // log_trace("enter Write");
  // 放入到合适的桶中
  u8 bucket_id = which_bucket(key[0]);
  Bucket &f = buckets[bucket_id];
  // pthread_rwlock_wrlock(&f.lock);
  pthread_mutex_lock(&f.lock);
  // log_trace("get lock");
  // {{{2 写入data文件, 而且必须先写data
  // struct stat st;
  // u32 before_size=st.st_size;
  int a_ret = FileAppend(f.data_fd, value.data(), value.size());
  Assert(a_ret==0, "data_fd=%d, value.size=%lu", f.data_fd, value.size()); // write value data
  // log_trace("write data done");
  // int ret = FileAppend(f.data_fd, value.data(), value.size()); // write value
  // data if(ret<0) {
  //   log_fatal("append data file fail, path=%s, id=%d",
  //   this->dir_name.c_str(), bucket_id); return kIOError;
  // }

  // 写入key_buf
  Location loc;
  loc.len = value.size();
  loc.offset = f.data_offset;

  u8 buf[80]; // TODO 这个80是随便定的, 再看看
  u8 *buf_pos = buf;
  *(u32 *)buf_pos = key.size();
  buf_pos += 4;

  memcpy(buf_pos, key.data(), key.size());
  buf_pos += key.size();

  *((Location *)buf_pos) = loc;
  buf_pos += location_sz;
  // 写入key文件(mmap)
  assert((u64)f.key_mmap_cur!=0);
  memcpy(f.key_mmap_cur, buf, buf_pos - buf); // 这是通过mmap来写的

  // 写入map
  f.map.insert_or_assign(key.ToString(), (Location*)(f.key_mmap_cur + 4 + key.size()));

  // 维护各个字段
  f.data_offset += value.size();
  f.key_mmap_cur = f.key_mmap_cur + 4 + key.size()+location_sz;
  // log_debug("finish Write, fd=%d, before data size=%u, f.data_offset=%u, data file size=%u, loc:offset=%u,len=%u, key=%s", f.data_fd, before_size, f.data_offset, (u32)st.st_size, loc.offset, loc.len, key.ToString().c_str());

  // pthread_rwlock_unlock(&f.lock);
  pthread_mutex_unlock(&f.lock);
  return kSucc;
}

// {{{1 Read
RetCode EngineRace::Read(const PolarString &key, std::string *value) {
  u8 bucket_id = which_bucket(key[0]);
  Bucket &f = buckets[bucket_id];
  // log_trace("enter Read, key=%s, bucket_id=%u", key.ToString().c_str(),(u32)bucket_id);
  RetCode ret = kNotFound;
  // pthread_rwlock_wrlock(&f.lock);
  pthread_mutex_lock(&f.lock);
  auto it = f.map.find(key);
  if (it != f.map.end()) {
    Location *loc = it->second;
    if(get_string_from_location(f.data_fd, loc, value) == 0) {
    ret = kSucc;
    } else {
      ret = kIOError;
    }
  }
  // pthread_rwlock_unlock(&f.lock);
  pthread_mutex_unlock(&f.lock);
  // log_trace("finish Read");
  return ret;
}

int EngineRace::get_string_from_location(i32 fd, Location* loc, std::string *value) {
  lseek(fd, loc->offset, SEEK_SET);
char buf[4097];
  char *pos = buf;
  u32 value_len = loc->len; // 在错误的这个地方, loc->len是7926335344292808279, 而value_len是120735319

  while (value_len > 0) {
    ssize_t r = read(fd, pos, value_len); // 这是在搞什么鬼
    Assert(r!=0, "fd=%d,loc_offset=%lu, loc_len=%lu", fd, loc->offset, loc->len);
    if (r < 0) {
      if (errno == EINTR) {
        log_trace("retry");
        continue; // Retry
      }
      perror("read_data_file fail");
      close(fd);
      return -1;
    }
    // log_trace("value_len=%u, r=%d",value_len, r);
    pos += r;
    value_len -= r;
  }
  *value = std::string(buf, loc->len);
  return 0;
}

RetCode EngineRace::Range(const PolarString &lower, const PolarString &upper,
                          Visitor &visitor) {

  u8 lo_bid=0, hi_bid=BUCKET_NUM-1;
  bool lo_nempty = (lower.size()!=0);
  bool hi_nempty = (upper.size()!=0);
  if(lo_nempty) {
    lo_bid = which_bucket(lower[0]);
  }
  if(hi_nempty) {
    hi_bid = which_bucket(upper[0]);
  }
  for(u8 i = lo_bid; i <= hi_bid; i++) {
    // pthread_rwlock_rdlock(&buckets[i].lock);
    pthread_mutex_lock(&buckets[i].lock);
  }
  u32 total_count = 0; // DEBUG
  for(u8 i = lo_bid; i <= hi_bid; i++) {
    u32 local_count = 0; // DEBUG
    Bucket &f = buckets[i];
    // 上界不为end, 只有一种情况: i是最后一个, 而且len还不为0
    auto lower_bound = f.map.cbegin();
    auto upper_bound = f.map.cend();
    if(i==hi_bid && hi_nempty) {
      upper_bound = f.map.lower_bound(upper.ToString());
    }
    if(i==lo_bid && lo_nempty) {
      lower_bound = f.map.lower_bound(lower.ToString());
    }
    std::string value;
    for(auto it = lower_bound; it != upper_bound; it++) {
      local_count++;
      get_string_from_location(f.data_fd, it->second, &value);
      visitor.Visit(it->first, value);
    }
    total_count+=local_count;
    // log_trace("bucket %u: count=%u", (u32)i, local_count);
  }
  // log_trace("totol count from %u to %u is %u", (u32)lo_bid, (u32)hi_bid, total_count);
  for(u8 i = lo_bid; i <= hi_bid; i++) {
    // pthread_rwlock_unlock(&buckets[i].lock);
    pthread_mutex_unlock(&buckets[i].lock);
  }
  return kSucc;
}

} // namespace polar_race
