#!/bin/bash

bench=('bench.cc')

rm -rf /tmp/ramdisk/data/test-*
for f in ${bench[@]}; do
    exe=$(echo $f | cut -d . -f1)
    echo $f
    g++ -std=c++17 -O0 -o  $exe -g -I.. -I $MHOME/Playground/lib/ubuntu/header $f  -L../lib -L $MHOME/Playground/lib/ubuntu/lib -DMOCK_NVM -lengine -lpthread -l log_c
done
