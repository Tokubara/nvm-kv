// Copyright [2018] Alibaba Cloud All rights reserved
#ifndef ENGINE_RACE_ENGINE_RACE_H_
#define ENGINE_RACE_ENGINE_RACE_H_
#include "include/engine.h"
#include <functional>
#include <iostream>
#include <pthread.h>
#include <string>
#include <map>
#define BUCKET_NUM 30

namespace polar_race {

using i8 = char;
using i32 = int;
using isize = ssize_t;
using u8 = unsigned char;
using u32 = unsigned;
using u64 = unsigned long;
using usize = size_t;

inline bool operator<(const PolarString& l, const PolarString& r)
{
  return l.compare(r) < 0;
}

struct Location {
  size_t offset; // byte offset in file
  size_t len;    // byte length of the data string
};

struct Bucket {
  std::map<std::string, Location*, std::less<>> map;
  i32 key_fd;
  i32 data_fd;
  u32 data_offset;
  u8* key_mmap;
  u8* key_mmap_cur;

  pthread_rwlock_t lock;
};
const u64 location_sz = sizeof(Location);

class EngineRace : public Engine {
  public:
  static const size_t key_file_size = 128 * 1024 * 1024; // 128MB
  Bucket buckets[BUCKET_NUM];
  std::string dir_name;

  static RetCode Open(const std::string& name, Engine** eptr);

  ~EngineRace() override;

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
  int read_data_file(i32 fd, Location *loc, char *buf);
};

} // namespace polar_race

#endif // ENGINE_RACE_ENGINE_RACE_H_
