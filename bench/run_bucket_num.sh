if [[ $# -lt 1 ]]; then
  echo "no arg(bucket_num)"
  exit 1
fi

rm -rf result_$1.csv
for threadNR in $(echo 1 $(seq 2 2 12)); do
  for readNR in {50,70,90,99}; do
    for isSkew in {0,1}; do
      ./bench_$1 $threadNR $readNR $isSkew >> result_$1.csv
      echo finish $threadNR $readNR $isSkew
    done
  done
done
