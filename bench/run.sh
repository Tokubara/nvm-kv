rm -rf result.csv
for threadNR in $(echo 1 $(seq 2 2 12)); do
  for readNR in {50,70,90,99}; do
    for isSkew in {0,1}; do
      ./bench $threadNR $readNR $isSkew >> result.csv
      echo finish $threadNR $readNR $isSkew
      # echo "$threadNR $readNR $isSkew >> result.csv"
    done
  done
done
