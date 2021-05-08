#!/bin/bash

bench=('bench.cc')

rm -rf /tmp/ramdisk/data/test-*
for f in ${bench[@]}; do
    exe=$(echo $f | cut -d . -f1)
    echo $f
    g++ -std=c++17 -o $exe -g -I.. $f  -L../lib -lengine -lpthread
done
