#!/bin/bash
# Usage: ./run.sh <L> <n>

if [ "$#" -lt 2 ]; then
    echo "Usage: ./run.sh <L> <n>"
    exit 1
fi

L=$1
N=$2
BIN_DIR="bin"
BASE_DIR="results"
TARGET_DIR="${BASE_DIR}/${L}"

# 建立目錄
mkdir -p "$TARGET_DIR"

SEED_FILE="${TARGET_DIR}/seed_L${L}.txt"
RESULT_FILE="${TARGET_DIR}/pacp_L${L}.txt"
LOG_FILE="${TARGET_DIR}/search_log.txt"

# 1. 設定目標 (Goal 1: Odd->2, Goal 2: Even->0)
TARGET_VAL=0
if [ $((L % 2)) -ne 0 ]; then TARGET_VAL=2; fi

# 2. 生成種子 (若無)
if [ ! -f "$SEED_FILE" ]; then
    "${BIN_DIR}/gen_seeds" $L "$SEED_FILE" $TARGET_VAL > /dev/null
fi

# 3. 執行搜尋
echo "[Start] L=$L | Target=$TARGET_VAL | Output: $RESULT_FILE" | tee -a "$LOG_FILE"

if [ "$L" -le 14 ]; then
    # 小長度：暴力法
    "${BIN_DIR}/brute_force" "$RESULT_FILE" $L | tee -a "$LOG_FILE"
else
    # 大長度：優化器
    # 策略判斷: 只有 "Prime & 3 mod 4" 使用 FixA
    IS_PRIME=$(python3 -c "n=$L; print(1 if all(n%i!=0 for i in range(2,int(n**0.5)+1)) and n>1 else 0)")
    FIX_A=0
    if [ "$IS_PRIME" -eq 1 ] && [ $((L % 4)) -eq 3 ]; then FIX_A=1; fi
    
    "${BIN_DIR}/optimizer" "$SEED_FILE" "$RESULT_FILE" $FIX_A $N $TARGET_VAL | tee -a "$LOG_FILE"
fi

echo "[Done]"