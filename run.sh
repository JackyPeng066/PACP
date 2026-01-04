#!/bin/bash
# ============================================================
# PACP Single Runner (The Worker)
# Logic:
#   - Odd L  (Goal 1): Target=2, Auto FixA for Primes
#   - Even L (Goal 2): Target=0, Only runs allowed Opt-PACP list
# ============================================================

# 1. 參數檢查
if [ "$#" -lt 2 ]; then
    echo "Usage: ./run.sh <L> <n>"
    echo "Example: ./run.sh 29 1   (Goal 1)"
    echo "Example: ./run.sh 28 1   (Goal 2)"
    exit 1
fi

L=$1
N=$2
BIN_DIR="bin"
BASE_DIR="results"
TARGET_DIR="${BASE_DIR}/${L}"

# 建立結果目錄
mkdir -p "$TARGET_DIR"

SEED_FILE="${TARGET_DIR}/seed_L${L}.txt"
RESULT_FILE="${TARGET_DIR}/pacp_L${L}.txt"
LOG_FILE="${TARGET_DIR}/search_log.txt"

# =======================================================
# [核心邏輯] 參數自動配置
# =======================================================

# Goal 2 白名單 (必須與 batch_run.sh 一致)
GOAL2_LIST=" 6 12 14 22 24 28 30 38 42 48 54 56 60 62 66 70 76 78 84 88 92 96 102 108 114 118 120 124 126 132 134 138 142 150 158 166 168 172 176 182 192 198 "

TARGET_VAL=0
FIX_A=0
STRATEGY_MSG=""

if [ $((L % 2)) -ne 0 ]; then
    # === Goal 1: 奇數 (Odd) ===
    TARGET_VAL=2
    
    # 判斷是否為質數 (用於決定 FixA 策略)
    # 若 L 是質數且 L = 3 mod 4 (如 3, 7, 11, 19, 23...) -> 鎖定 A (Legendre)
    IS_PRIME=$(python3 -c "n=$L; print(1 if all(n%i!=0 for i in range(2,int(n**0.5)+1)) and n>1 else 0)")
    
    if [ "$IS_PRIME" -eq 1 ] && [ $((L % 4)) -eq 3 ]; then 
        FIX_A=1
        STRATEGY_MSG="Legendre (Fix A)"
    else
        FIX_A=0
        STRATEGY_MSG="Co-evolution (Random)"
    fi

else
    # === Goal 2: 偶數 (Even) ===
    # 嚴格檢查：只允許 Opt-PACP 清單內的數字
    if [[ "$GOAL2_LIST" =~ " $L " ]]; then
        TARGET_VAL=0
        FIX_A=0
        STRATEGY_MSG="Opt-PACP Search (Target 0)"
    else
        echo "========================================================"
        echo "[Error] L=$L is NOT in the allowed Opt-PACP list."
        echo "This script refuses to run non-target even lengths."
        echo "Allowed: $GOAL2_LIST"
        echo "========================================================"
        exit 1
    fi
fi

# =======================================================
# [準備工作]
# =======================================================

# 2. 生成種子 (若檔案不存在)
if [ ! -f "$SEED_FILE" ]; then
    # 根據前面的 TARGET_VAL 決定種子類型
    "${BIN_DIR}/gen_seeds" $L "$SEED_FILE" $TARGET_VAL > /dev/null
fi

# 3. 處理 N=0 (無限模式)
# 傳給 C++ 的次數。若 N=0，傳入 INT_MAX (約 20億)，讓 C++ 跑到天荒地老
PASS_N=$N
if [ "$N" -eq 0 ]; then
    PASS_N=2147483647
fi

# =======================================================
# [執行程式]
# =======================================================

# 寫入日誌 header
TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
echo "[$TIMESTAMP] Start L=$L | Target=$TARGET_VAL | Mode=$STRATEGY_MSG | Count=$N" | tee -a "$LOG_FILE"

if [ "$L" -le 14 ]; then
    # 小長度：使用暴力法 (Brute Force)
    # 注意：暴力法通常不支援 FixA 參數，直接跑全域搜尋
    "${BIN_DIR}/brute_force" "$RESULT_FILE" $L | tee -a "$LOG_FILE"
else
    # 大長度：使用優化器 (Optimizer)
    # 指令格式: ./optimizer <In> <Out> <FixA> <Count> <Target>
    "${BIN_DIR}/optimizer" "$SEED_FILE" "$RESULT_FILE" $FIX_A $PASS_N $TARGET_VAL | tee -a "$LOG_FILE"
fi

echo "[Done]"