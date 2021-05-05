#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <mylib.h>

#include "include/engine.h"
#include "test_util.h"

using namespace polar_race;

struct CountVisitor : Visitor {
  int &count;
  CountVisitor(int &count) : count(count) {}
  void Visit(const PolarString &, const PolarString &) override {
    ++count;
  }
};

#define KV_CNT 10000

char k[1024];
char v[9024];
std::string ks[KV_CNT];
std::string vs[KV_CNT];
int main() {

    Engine *engine = NULL;
    printf_(
        "======================= range test "
        "============================");
    std::string engine_path =
        std::string("./data/test-") + std::to_string(asm_rdtsc());
    RetCode ret = Engine::Open(engine_path, &engine);
    assert(ret == kSucc);
    printf("open engine_path: %s\n", engine_path.c_str());

    std::string value;

    for (int i = 0; i < KV_CNT; ++i) {
        gen_random(k, 6);
        ks[i] = std::string(k) + std::to_string(i);
        gen_random(v, 1027);
        vs[i] = v;
    }

    std::sort(ks, ks + KV_CNT); // 也就是说, ks在此时就已经是有序的

    for (int i = 0; i < KV_CNT; ++i) {
        ret = engine->Write(ks[i], vs[i]); // 是按顺序写的key, 其实这个vs一点也不重要
        assert(ret == kSucc);
    }
    // 生成两个随机数, hi, lo, 测试数目相同 
    for (int i = 0; i < 1000; ++i) {
        int lo = rand() % KV_CNT, hi = rand() % KV_CNT;
        if (lo > hi) {
            std::swap(lo, hi);
        }
        int cnt = 0;
        CountVisitor v(cnt);
        ret = engine->Range(ks[lo], ks[hi], v);
        assert(ret == kSucc);
        log_trace("cnt=%d,hi-lo=%d",cnt,hi-lo);
        assert(cnt == hi - lo);
    }
  // 左边没有, 右边有
    for (int i = 0; i < 1000; ++i) {
        int hi = rand() % KV_CNT;
        int cnt = 0;
        CountVisitor v(cnt);
        ret = engine->Range("", ks[hi], v);
        assert(ret == kSucc);
        assert(cnt == hi);
    }
   // 左边有, 右边没有
    for (int i = 0; i < 1000; ++i) {
        int lo = rand() % KV_CNT;
        int cnt = 0;
        CountVisitor v(cnt);
        ret = engine->Range(ks[lo], "", v);
        assert(ret == kSucc);
        assert(cnt == KV_CNT - lo);
    }
  // 两边都没有
    {
        int cnt = 0;
        CountVisitor v(cnt);
        ret = engine->Range("", "", v);
        assert(ret == kSucc);
        assert(cnt == KV_CNT);
    }

    printf_(
        "======================= range test pass :) "
        "======================");

    return 0;
}
