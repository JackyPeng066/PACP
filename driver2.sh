#!/bin/bash

[ "$#" -ne 1 ] && { echo "Usage: $0 <L>"; exit 1; }
L=$1
START_TIME=$(date +%s)

# 1. 動態計算目標總和 (活性來源)
# 奇數長度目標為 2 (Goal 1), 偶數長度目標為 4 (Goal 2/3)
if [ $((L % 2)) -ne 0 ]; then
    TARGET_SUM=2
else
    TARGET_SUM=4
fi

# 2. 自動編譯
g++ -O3 src/gen_fast.cpp -o bin/gen_fast
g++ -O3 src/match_fast.cpp -o bin/match_fast

# 3. 動態權重分配
./bin/aps_weight $L
W_FILE="results/$L/${L}_weight.txt"
READ_PAIR=$(head -n 1 "$W_FILE" | tr -d '\r')
read -r g0 g1 <<< "$READ_PAIR"

DIR="results/$L/${g0}_${g1}"
mkdir -p "$DIR"

echo ">>> DYNAMIC SEARCH | L=$L | TARGET_SUM=$TARGET_SUM | PAIR=($g0,$g1)"

# 4. 生成與串流碰撞 (將 TARGET_SUM 傳入 code 作為唯一活性指標)
./bin/gen_fast $L $g1 $TARGET_SUM > "$DIR/poolB.bin"
./bin/gen_fast $L $g0 $TARGET_SUM | ./bin/match_fast $L "$DIR/poolB.bin" $TARGET_SUM > "$DIR/match_result.txt"

# 5. 過濾與報告
if [ -s "$DIR/match_result.txt" ]; then
    ./bin/aps_filter $L $g0 $g1
else
    echo "No valid PACP pairs found for $L."
    exit 0
fi

# 報告格式保持一致
echo "-------------------------------------------------------"
printf "%-15s | %-12s | %-12s\n" "Weight($g0,$g1)" "RawPairs" "UniquePairs"
RAW=$(grep -c "[+-]" "$DIR/match_result.txt" 2>/dev/null || echo 0)
UNIQ=$(grep -c "[+-]" "$DIR/unique_result.txt" 2>/dev/null || echo 0)
printf "%-15s | %-12s | %-12s\n" "($g0,$g1)" "$RAW" "$UNIQ"
echo "-------------------------------------------------------"
echo "Total Time: $(($(date +%s) - START_TIME))s"