#include <assert.h>
#include <stdio.h>

#include <string>
#include <thread>
#include <mylib.h>

#include "include/engine.h"
#include "test_util.h"

using namespace polar_race;

#define KV_CNT 1000
#define THREAD_NUM 2
#define CONFLICT_KEY 50

char k[1024];
char v[9024];
std::string ks[THREAD_NUM][KV_CNT];
std::string vs[THREAD_NUM][KV_CNT];
Engine *engine = NULL;
// {{{1 写后立即读
void test_thread(int id) {
    RetCode ret;
    std::string value;
    for (int i = 0; i < KV_CNT; ++i) {
        ret = engine->Write(ks[id][i], vs[id][i]);
        assert(ret == kSucc);
        // log_trace("thread_id=%d,i=%d",id,i);
        ret = engine->Read(ks[id][i], &value);
        assert(ret == kSucc);
        Assert(value == vs[id][i], "id=%d, i=%d",id, i);
    }
}

// {{{1 id号线程同时写ks[0][i], vs[id][i], 也就是说,写入的key都是一样的, 但是value不一样
void test_thread_conflict(int id) {
    RetCode ret;
    std::string value;

    for (int k = 0; k < 10000; ++k) {
        for (int i = 0; i < CONFLICT_KEY; ++i) {
            ret = engine->Write(ks[0][i], vs[id][i]);
            assert(ret == kSucc);
        }

    }
}

int main() {

    printf_(
        "======================= multi thread test "
        "============================");
    log_set_level(LOG_DEBUG);
#ifdef MOCK_NVM
    system("rm -rf /tmp/ramdisk/data/*"); // 否则1g存储, 根本不够用, posix_fallocate会报错
    std::string engine_path =
        std::string("/tmp/ramdisk/data/test-") + std::to_string(asm_rdtsc());
#else
    // std::string engine_path = "/dev/dax0.0";
     system("rm -rf data/*");
    std::string engine_path = std::string("data/test-") + std::to_string(asm_rdtsc());
#endif
    RetCode ret = Engine::Open(engine_path, &engine);
    assert(ret == kSucc);
    printf("open engine_path: %s\n", engine_path.c_str());

    // {{{1 生成数据, ks,vs, 第一维是进程数, 有4个线程, 第二维是kv对数, 是10000
    for (int t = 0; t < THREAD_NUM; ++t) {
        for (int i = 0; i < KV_CNT; ++i) {
            gen_random(k, 6);
            ks[t][i] = std::to_string(t) + "th" + std::string(k) + std::to_string(i);

            gen_random(v, 1027);
            vs[t][i] = v;
        }
    }

    std::thread ths[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; ++i) {
        ths[i] = std::thread(test_thread, i);
    }
    for (int i = 0; i < THREAD_NUM; ++i) {
        ths[i].join();
    }
    puts("finish phase0");
// {{{1 再读所有的
    std::string value;
    for (int t = 0; t < THREAD_NUM; ++t) {
        for (int i = 0; i < KV_CNT; ++i) {
            ret = engine->Read(ks[t][i], &value);
            assert(ret == kSucc);
            assert(value == vs[t][i]);
        }
    }
    puts("finish phase1");
    ////////////////////////////////////////////////////////////////////
    for (int i = 0; i < THREAD_NUM; ++i) {
        ths[i] = std::thread(test_thread_conflict, i);
    }
    for (int i = 0; i < THREAD_NUM; ++i) {
        ths[i].join();
    }
    puts("finish phase2");
    // {{{1 对thread_conflict的检测, value一定要与4个进程之一的相同
    for (int i = 0; i < CONFLICT_KEY; ++i) {
        ret = engine->Read(ks[0][i], &value);
        assert(ret == kSucc);

        bool found = false;
        for (int t = 0; t < THREAD_NUM; ++t) {
            if (value == vs[t][i]) {
                found = true;
                break;
            }
        }
        assert(found);
    }
    puts("finish phase3");

    delete engine;


    printf_(
        "======================= multi thread test pass :) "
        "======================");

    return 0;
}
