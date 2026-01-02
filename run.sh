#!/bin/bash
if [ "$#" -lt 2 ]; then echo "Usage: ./run.sh <L> <n>"; exit 1; fi

L=$1
N=$2
DIR="results/${L}"
mkdir -p "$DIR"
OUT="${DIR}/pacp_L${L}.txt"
LOG="${DIR}/log.txt"

# 編譯
g++ -O3 -std=c++17 -o gen_seed src/gen_seeds.cpp lib/pacp_core.cpp
g++ -O3 -std=c++17 -o optimizer src/optimizer.cpp lib/pacp_core.cpp
g++ -O3 -std=c++17 -o brute_force src/brute_force.cpp lib/pacp_core.cpp

# 生成種子
./gen_seed $L "${DIR}/seed.txt" > /dev/null

# 決策
TARGET_VAL=0
if [ $((L % 2)) -ne 0 ]; then TARGET_VAL=2; fi

echo "[Start] L=$L Target=$TARGET_VAL"
echo "Check $LOG for details."

if [ "$L" -le 15 ]; then
    ./brute_force "$OUT" $L > "$LOG"
else
    # 判斷質數略...假設FIX_A邏輯同前
    # 這裡簡化演示
    FIX_A=0 
    ./optimizer "${DIR}/seed.txt" "$OUT" $FIX_A $N $TARGET_VAL > "$LOG"
fi

echo "[Finished] See $OUT"