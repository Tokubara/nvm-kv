// Copyright [2018] Alibaba Cloud All rights reserved
#include "engine_race.h"
#include "util.h"
#include <fcntl.h>
#include <iostream>
#include <map>
#include <mylib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
namespace polar_race {

int EngineRace::fd_index_tmp[EngineRace::BUCKET_NUM] = {};

RetCode Engine::Open(const std::string &name, Engine **eptr) {
  return EngineRace::Open(name, eptr);
}

Engine::~Engine() {}

// read (key_size, key, location)
// keysize key(不含\0), value所在的Location
// RetCode EngineRace::read_index_file(int bucket_id, int& fd)
//{ // fd并不会改变, 我觉得这里存const fd也行
//  ssize_t rd;
//  int sz;
//  char buf[1024];
//  while ((rd = read(fd, (char*)&sz, 4)) > 0) { // key size is a 4-byte int
//    if (rd != 4) {
//      fprintf(stderr, "wrong index file, cannot read key size\n");
//      return kIOError;
//    }
//    if (sz == 0) // no records any more
//    {
//      rd = 0;
//      break;
//    }
//    // read key
//    rd = read(fd, buf, sz);
//    if (rd != sz) {
//      fprintf(stderr, "wrong index file, cannot read key\n");
//      return kIOError;
//    }
//    buf[sz] = 0;
//    std::string key(buf);

//    //read location
//    Location pos;
//    rd = read(fd, (char*)&(pos), sizeof(Location));
//    if (rd != sizeof(Location)) {
//      fprintf(stderr, "wrong index file, cannot read location\n");
//      return kIOError;
//    }
//    this->index[bucket_id][key] = pos;
//  }
//  if (rd == 0) // end of file
//    return kSucc;
//  if (rd < 0)
//    return kIOError;
//  return kSucc;
//}

// 1. Open engine
// check if index.tmp exists
// if yes, maybe a crash just happened, combine the index
// if no, load index
RetCode EngineRace::Open(const std::string &name, Engine **eptr) {
  log_trace("Open");
  *eptr = NULL;
  EngineRace *engine = new EngineRace(name);
  std::string buf = "mkdir -p " + name;
  if (system(buf.c_str()) != 0) {
    return kIOError;
  }
  // {{{2 会被用于设置字段的变量
  u32 data_offset = 0;
  u8 *data = nullptr;
  u8 *cur = nullptr;
  i32 data_fd = -1, key_fd = -1;

  struct stat st; // 也是循环中临时变量的性质
  // init every bucket
  for (size_t i = 0; i < BUCKET_NUM; i++) {
    u32 key_count = 0;
    std::string suffix = std::to_string(i);
    // int fd_index;
    Buckect &f = engine->buckets[i];
    key_fd = open(name + "/index_" + suffix, O_RDWR, 0666);
    // {{{2 目录已存在
    if (key_fd > 0) {
      // 维护各个字段
      // 再打开数据文件
      data_fd = open(name + "/data_" + suffix, O_RDWR, 0666);
      if (data_fd < 0) {
        log_error("cannot open data file, path=%s, id=%d", name.c_str(), i);
        perror("open data file fail");
        return kIOError;
      }
      assert(fstat(data_fd, &st) == 0);
      data_offset = st.st_size;

      // mmap
      assert(fstat(key_fd, &st)); // 只是为了获得文件大小
      assert(st.st_size == key_file_size);
      data = (u8 *)mmap(nullptr, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        key_fd, 0);
      if (data == MAP_FAILED) {
        perror("mmap fail");
        return kIOError;
      }
      // 解析index文件
      u8 *cur = data; // 表示已经读取的index文件的位置
      while (true) {
        u32 key_sz = *(u32 *)cur;

        if (key_sz == 0) {
          log_trace("index file finish, key_count:%u", key_count);
          break;
        } else {
          f.map.insert_or_assign(std::string(cur + 4, key_sz),
                                 (Location *)(cur + 4 + key_sz));
          cur = cur + 4 + key_sz + location_sz;
          key_count++; // DEBUG
        }
      }

    } else {                                         // {{{2 目录不存在
      assert(!FileExists(name + "/data_" + suffix)); // 数据文件也应该不存在
      // 新目录, 创建data和index文件
      key_fd = open(name + "/index_" + suffix, O_RDWR | O_CREAT, 0666);
      if (key_fd < 0) {
        perror("cannot create file");
        return kIOError;
      }
      data_fd = open(name + "/data_" + suffix, O_RDWR | O_CREAT, 0666);
      if (data_fd < 0) {
        perror("cannot open file");
        return kIOError;
      }
      data_offset = 0;
      if (posix_fallocate(key_fd, 0, key_file_size) != 0) {
        perror("cannot posix_fallocate");
        return kIOError;
      }
      // 创建index的映射数组, 初始化与映射相关的各个字段
      data = (u8 *)mmap(NULL, key_file_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        key_fd, 0);
      if (data == MAP_FAILED) {
        perror("mmap fail");
        return kIOError;
      }
      cur = data;
    }
    // {{{2 公共字段
    // 维护各个字段(好像也就4个字段)
    f.key_mmap = data;
    f.key_mmap_cur = cur;
    f.key_fd = key_fd;
    f.data_fd = data_fd;
    f.data_offset = data_offset;
    // 初始化锁
    pthread_rwlock_init(&f.lock, nullptr);
  }
  *eptr = engine;
  return kSucc;
}

// {{{1 析构
EngineRace::~EngineRace() {
  log_trace("Saving index");
  for (size_t i = 0; i < BUCKET_NUM; i++) {
    Bucket &f = buckets[i];
    munmap(f.key_mmap, key_file_size); // 因为的确用不着了, 信息都在index中
    close(f.key_fd);
    close(f.data_fd);
    pthread_rwlock_destroy(&f.lock);
  }
}

// {{{1 write
// 需要维护的是2部分: index数组, 还有写文件, 不需要直接write, 是通过mmap的方式,
// memcpy就行了
RetCode EngineRace::Write(const PolarString &key, const PolarString &value) {
  // 放入到合适的桶中
  size_t bucket_id = (u8)key[0] % BUCKET_NUM;
  Bucket &f = buckets[bucket_id];
  pthread_rwlock_wrlock(&f.lock);
  // {{{2 写入data文件, 而且必须先写data
  assert(FileAppend(f.data_fd, value.data(), value.size()) ==
         0); // write value data
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
  (u32 *)buf_pos = key.size();
  buf_pos += 4;

  memcpy(buf_pos, key.data(), key.size());
  buf_pos += key.size();

  *((Location *)buf_pos) = loc;
  buf_pos += location_sz;
  // 写入key文件(mmap)
  memcpy(f.key_mmap_cur, buf, buf_pos - buf); // 这是通过mmap来写的

  // 写入map
  f.map.insert_or_assign(key.ToString(), f.key_mmap_cur + 4 + key.size());

  // 维护各个字段
  f.data_offset += value.size();
  f.key_mmap_cur = f.kep_mmap_cur + 4 + key.size();

  pthread_rwlock_unlock(&f.lock);
  return kSucc;
}

// {{{1 Read
RetCode EngineRace::Read(const PolarString &key, std::string *value) {
  size_t bucket_id = (u8)key[0] % BUCKET_NUM;
  Bucket &f = buckets[bucket_id];
  RetCode ret = kNotFound;
  pthread_rwlock_rdlock(&f.lock);
  auto it = f.map.find(key);
  if (it != f.map.end()) {
    ret = kNotFound;
  } else {
    char buf[4097]; // TODO value的大小到底是多少?可能需要改
    Location *loc = it->second;
    assert(read_data_file(f.data_fd, loc, buf) == 0);
    *value = std::string(buf, loc->len);
  }
  pthread_rwlock_unlock(&f.lock);
  return kSucc;
}
// seek and read
size_t EngineRace::read_data_file(i32 fd, Location *loc, char *buf) {
  lseek(fd, loc->offset, SEEK_SET);
  char *pos = buf;
  u32 value_len = l->len;

  while (value_len > 0) {
    ssize_t r = read(fd, buf, value_len);
    if (r < 0) {
      if (errno == EINTR) {
        continue; // Retry
      }
      perror("read_data_file fail");
      close(fd);
      return -1;
    }
    pos += r;
    value_len -= r;
  }
  return 0;
}
/*
 * NOTICE: Implement 'Range' in quarter-final,
 *         you can skip it in preliminary.
 */
// 5. Applies the given Vistor::Visit function to the result
// of every key-value pair in the key range [first, last),
// in order
// lower=="" is treated as a key before all keys in the database.
// upper=="" is treated as a key after all keys in the database.
// Therefore the following call will traverse the entire database:
//   Range("", "", visitor)
RetCode EngineRace::Range(const PolarString &lower, const PolarString &upper,
                          Visitor &visitor) {
  return kSucc;
}

} // namespace polar_race
