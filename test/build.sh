#!/bin/bash

test=('single_thread_test.cc' 'multi_thread_test.cc' 'crash_test.cc' 'range_test.cc')
# test=('multi_thread_test.cc')
# test=('single_thread_test.cc' 'multi_thread_test.cc' 'crash_test.cc')

rm -rf data/test-*
# rm -rf /tmp/ramdisk/data/test-*
for f in ${test[@]}; do
    exe=$(echo $f | cut -d . -f1)
    echo $f
    # g++ -std=c++17 -O0 -o  $exe -g -I.. $f  -L../lib -L $MHOME/Playground/lib/ubuntu/lib  -lengine_debug -lpthread -DMOCK_NVM -l log_c
    g++ -std=c++17 -O0 -o  $exe -g -I.. -I $MHOME/Playground/lib/ubuntu/header $f  -L../lib -L $MHOME/Playground/lib/ubuntu/lib -DMOCK_NVM -lengine -lpthread -l log_c
done
