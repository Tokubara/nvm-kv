// Copyright [2018] Alibaba Cloud All rights reserved
#ifndef ENGINE_RACE_ENGINE_RACE_H_
#define ENGINE_RACE_ENGINE_RACE_H_
#include "include/engine.h"
#include <fcntl.h>
#include <map>
#include <pthread.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

namespace polar_race {
using i8 = char;
using i32 = int;
using isize = ssize_t;
using u8 = unsigned char;
using u32 = unsigned;
using usize = size_t;

inline bool operator<(const PolarString &l, const PolarString &r) { //? TODO 可我觉得这个方法并不需要
  return l.compare(r) < 0;
}

const u32 HASH_SIZE = 256;
// must define as usize, if use u32, (usize) ~PAGE_SIZE_M1 will be 0x0000000011111000
const usize PAGE_SIZE_M1 = 4095;

struct File {
  // by explicitly giving std::less<>, which accepts different comparison parameter type
  // plus the generic std::map::find in C++ 14
  // we can search in `map` by a PolarString, without constructing a std::string
  std::map<std::string, std::string, std::less<>> map; //? 这个map是干啥的? less是啥?
  i32 fd;
  pthread_rwlock_t lock;
};

class EngineRace : public Engine {
public:
  static RetCode Open(const std::string &name, Engine **eptr) {
    std::string buf = "mkdir -p " + name;
    if (system(buf.c_str()) != 0) { return kIOError; } // 创建目录的简单方式
    EngineRace *ret = new EngineRace;
    buf = name + "/00";
    u32 id_pos = buf.size() - 2; // 那么id应该就是最后2位
    for (u32 i = 0; i < HASH_SIZE; ++i) {
      buf[id_pos] = (i >> 4) + 'a', buf[id_pos + 1] = (i & 15) + 'a'; // 意思是文件名是这样决定的: id/16, 得到的商, 从'a'开始顺延, 就是名字的第1位, 得到的余数从'a'开始顺延, 就是名字的第2位. 注意到, 这是取不到全部的a-z的, 只能取到从'a'开始的16个字符
      File &f = ret->fs[i];
      do { // 如果文件存在, 那么读取它的kv, 存入map, 否则直接创建
        i32 fd = open(buf.c_str(), O_RDONLY, 0666); // 这里是只读, 那只是暂时的, 只读的部分是用于初始化map //? TODO 但是有个问题: 它没有close, 但我觉得应该有close
        if (fd < 0) { break; }
        struct stat st;
        fstat(fd, &st); // 只是为了获得文件大小
        i8 *data = (i8 *)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // TODO 还是用了mmap, 不过为什么需要用mmap? 而且也是读映射
        for (i8 *cur = data, *end = data + st.st_size; cur < end;) { // 读取整个文件
          u32 key_sz = *(u32 *)cur; // kv是这么放的: 第一个数, 4字节, 是key_size, 然后是key, 然后是value_size, 然后是value
          u32 val_sz = *(u32 *)(cur + 4 + key_sz); // +4表示的是放的第一个是key_size
          f.map.insert_or_assign(std::string(cur + 4, key_sz), std::string(cur + 8 + key_sz, val_sz)); // 往map插入了key,value
          cur = (i8 *)((usize)(cur + 8 + key_sz + val_sz + PAGE_SIZE_M1) & ~PAGE_SIZE_M1); // kv一定是从以一整页开始的
        }
      } while (false);
      f.fd = open(buf.c_str(), O_RDWR | O_CREAT | O_APPEND |O_SYNC |O_DIRECT, 0666); // 就算是文件存在, 这一步也是需要的, 注意是APPEND
      // assert(f.fd>0);
      if(f.fd<0) {
        perror("open file fail");
        // fflush(NULL);
        exit(-1);
      } else {
        // puts("open file succeed");
        // fflush(NULL);
      }
      pthread_rwlock_init(&f.lock, nullptr);
    }
    *eptr = ret;
    return kSucc;
  }

  ~EngineRace() {
    for (File &f : fs) {
      close(f.fd);
      pthread_rwlock_destroy(&f.lock);
    }
  }

  RetCode Write(const PolarString &key, const PolarString &val) override {
    const u32 BUF_SZ = 1024 * 1024; //? 为什么buf是这么大
    thread_local u8 BUF[BUF_SZ + PAGE_SIZE_M1]; // I tried alignas(4096), it seems not working, so I have to manually align it
    if (key.empty()) { return kInvalidArgument; }
    File &f = fs[(u8)key[0]]; // 也就是说, 它属于哪个文件是这样决定的: key的第一个字符. 但是有个问题, 这样并不能分均匀吧. key的首字符如果都是英文数字字符, 那么会用到的文件就那么一些. 而且即使不是英文数字字符, 从128到256的也都用不上啊
    u32 key_sz = key.size(), val_sz = val.size();
    u32 tot_sz = (8 + key_sz + val_sz + PAGE_SIZE_M1) & ~PAGE_SIZE_M1; // 在文件所占的大小
    if (tot_sz > BUF_SZ) { return kInvalidArgument; }
    u8 *buf = (u8 *)((usize)(BUF + PAGE_SIZE_M1) & ~PAGE_SIZE_M1); // 得到的buf似乎是BUF的地址向4096对齐. BUF的地址很可能不是4096对齐的
    *(u32 *)buf = key_sz;
    memcpy(buf + 4, key.data(), key_sz);
    *(u32 *)(buf + 4 + key_sz) = val_sz;
    memcpy(buf + 8 + key_sz, val.data(), val_sz);
    if (write(f.fd, buf, tot_sz) != tot_sz) {
      // if IO cannot be performed in one `write` call, there will be synchronization problems
      return kIncomplete;
    }
    std::string owned_key(key.data(), key_sz), owned_val(val.data(), val_sz);
    pthread_rwlock_wrlock(&f.lock);
    f.map.insert_or_assign(std::move(owned_key), std::move(owned_val));
    pthread_rwlock_unlock(&f.lock);
    return kSucc;
  }

  RetCode Read(const PolarString &key, std::string *value) override {
    if (key.empty()) { return kInvalidArgument; }
    File &f = fs[(u8)key[0]];
    RetCode ret = kNotFound;
    pthread_rwlock_rdlock(&f.lock);
    auto it = f.map.find(key);
    if (it != f.map.end()) {
      ret = kSucc;
      *value = it->second;
    }
    pthread_rwlock_unlock(&f.lock);
    return ret;
  }

  RetCode Range(const PolarString &lo, const PolarString &hi, Visitor &vis) override {
    u32 lo_hash = lo.empty() ? 0 : lo[0];
    u32 hi_hash = hi.empty() ? HASH_SIZE - 1 : hi[0]; // 这两个hash值, 只不过是确定了文件的范围
    for (u32 i = lo_hash; i <= hi_hash; ++i) {
      pthread_rwlock_rdlock(&fs[i].lock);
    }
    if (lo_hash == hi_hash) { //? 为什么单独分开这两种情况? 似乎是因为, 但我感觉不止, 否则即使只对应了单个文件, 还是可以用下面的for循环, 但我怀疑这两种情况本来就可以合并
      auto &m = fs[lo_hash].map;
      auto it_pair = std::pair(m.lower_bound(lo), hi.empty() ? m.end() : m.lower_bound(hi)); // 这是个pair, 但它真正的意义是一个左闭右开的区间
      // 为什么lo为空, 不需要考虑lower_bound, 因为对于空字符, 它的lower_bound, 肯定就是第一个, 是一致的, 但是对于high, 不应该是第一个
      for (auto it = it_pair.first; it != it_pair.second; ++it) {
        vis.Visit(it->first, it->second);
      }
    } else {
      for (u32 i = lo_hash; i <= hi_hash; ++i) { // 遍历所有的文件
        auto &m = fs[i].map;
        auto it_pair = i == lo_hash ? std::pair(m.lower_bound(lo), m.end())
                                    : i == hi_hash ? std::pair(m.begin(), hi.empty() ? m.end() : m.lower_bound(hi))
                                                   : std::pair(m.begin(), m.end());
        for (auto it = it_pair.first; it != it_pair.second; ++it) {
          vis.Visit(it->first, it->second);
        }
      }
    }
    for (u32 i = lo_hash; i <= hi_hash; ++i) {
      pthread_rwlock_unlock(&fs[i].lock);
    }
    return kSucc;
  }

private:
  File fs[HASH_SIZE];
};
} // namespace polar_race

#endif // ENGINE_RACE_ENGINE_RACE_H_
