alias out_nvm="rsync_all -m u -f ./engine_race -t nvm-kv"
# alias in="rsync_all -m u -r -f kv -t ../nvm-kv"
alias out="rsync_all -m u -f ../kv"
alias in="rsync_all -m u -r -f kv -t ../kv"
cp -r ./engine_race ../../nvm-kv


# 交换两个库
mv ~/kv/lib ~/nvm-kv/tmplib
mv ~/nvm-kv/lib ~/kv/
mv ~/nvm-kv/tmplib ~/nvm-kv/lib
