#!/bin/bash

# test=('single_thread_test.cc' 'multi_thread_test.cc' 'crash_test.cc' 'range_test.cc')
test=('single_thread_test.cc' 'multi_thread_test.cc' 'crash_test.cc')
# test=('single_thread_test.cc')

rm -rf /tmp/ramdisk/data/test-*
for f in ${test[@]}; do
    exe=$(echo $f | cut -d . -f1)
    echo $f
    g++ -std=c++14 -O0 -o  $exe -g -I.. $f  -L../lib -L $MHOME/Playground/lib/ubuntu/lib  -lengine_debug -lpthread -DMOCK_NVM -l log_c
done
