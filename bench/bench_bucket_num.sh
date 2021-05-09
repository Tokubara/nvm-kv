#!/bin/bash

if [[ $# -lt 1 ]]; then
  echo "no arg(bucket_num)"
  exit 1
fi

cd ..
rm -rf lib
make DFLAGS="-DBUCKET_NUM=$1"

cd bench
rm -rf *log.txt
bench=('bench.cc')

rm -rf /tmp/ramdisk/data/test-*
for f in ${bench[@]}; do
    exe=$(echo $f | cut -d . -f1)
    echo $f
    # g++ -std=c++17 -O0 -o  $exe -g -I.. -I $MHOME/Playground/lib/ubuntu/header $f  -L../lib -L $MHOME/Playground/lib/ubuntu/lib -DMOCK_NVM -lengine -lpthread -l log_c
    g++ -std=c++11 -O2 -o  ${exe}_${1} -I.. -I $MHOME/Playground/lib/ubuntu/header $f  -L../lib -DMOCK_NVM -lengine -lpthread
done
