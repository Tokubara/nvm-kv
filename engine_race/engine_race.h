// Copyright [2018] Alibaba Cloud All rights reserved
#ifndef ENGINE_RACE_ENGINE_RACE_H_
#define ENGINE_RACE_ENGINE_RACE_H_
#include "include/engine.h"
#include <functional>
#include <iostream>
#include <pthread.h>
#include <string>
#include <unordered_map>
#define BUCKET_NUM 30

namespace polar_race {

using i8 = char;
using i32 = int;
using isize = ssize_t;
using u8 = unsigned char;
using u32 = unsigned;
using usize = size_t;

inline bool operator<(const PolarString& l, const PolarString& r)
{
  return l.compare(r) < 0;
}
struct Bucket {
  std::map<std::string, Location*, std::less<>> map;
  i32 key_fd;
  i32 data_fd;
  u8* key_mmap;
  u8* key_mmap_cur;

  pthread_rwlock_t lock;
}

struct Location {
  size_t offset; // byte offset in file
  size_t len;    // byte length of the data string
};

const u64 location_sz = sizeof(Location);

class EngineRace : public Engine {
  public:
  static const size_t chunckSize = 128 * 1024 * 1024; // 128MB
  Bucket buckets[BUCKET_NUM];
  std::string dir_name;

  static RetCode Open(const std::string& name, Engine** eptr);

  explicit EngineRace(const std::string& dir)
      : dir_name(dir)
  {
  }

  ~EngineRace();

  RetCode Write(const PolarString& key,
      const PolarString& value) override;

  RetCode Read(const PolarString& key,
      std::string* value) override;

  /*
   * NOTICE: Implement 'Range' in quarter-final,
   *         you can skip it in preliminary.
   */
  RetCode Range(const PolarString& lower,
      const PolarString& upper,
      Visitor& visitor) override;

  // private: // DEBUG

  // helper function to read data file
  size_t read_data_file(size_t bucket_id, Location& loc, char* buf);

  RetCode read_index_file(int bucket_id, int& fd);

  // hash: stirng --> one bucket
  size_t calculate_bucket_id(const std::string& key)
  {
    std::hash<std::string> hash_func;
    return hash_func(key) % thread_cnt;
  }
};

} // namespace polar_race

#endif // ENGINE_RACE_ENGINE_RACE_H_
