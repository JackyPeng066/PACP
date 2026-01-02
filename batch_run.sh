#!/bin/bash

# ============================================================
# PACP Batch Runner (Custom Range & Limits)
# Automates run.sh for a range of Odd Lengths (L)
# ============================================================

# 1. 參數檢查與 Usage 提示
if [ "$#" -lt 4 ]; then
    echo "========================================================"
    echo "Usage: ./batch_run.sh <Start_L> <End_L> <Count> <Timeout>"
    echo "--------------------------------------------------------"
    echo "  <Start_L> : Start length (e.g., 29)"
    echo "  <End_L>   : End length (e.g., 99)"
    echo "  <Count>   : Target pairs to find per L (e.g., 1)"
    echo "  <Timeout> : Max seconds per L (e.g., 60)"
    echo "========================================================"
    echo "Example: ./batch_run.sh 29 99 1 60"
    exit 1
fi

START_L=$1
END_L=$2
TARGET_N=$3
TIME_LIMIT=$4

# 2. 預先編譯
echo "[Batch] Pre-compiling executables..."
make
if [ $? -ne 0 ]; then
    echo "[Error] Make failed. Aborting."
    exit 1
fi

echo "[Batch] Starting loop from L=$START_L to $END_L (Odd only)"
echo "[Batch] Target: $TARGET_N pair(s) | Timeout: ${TIME_LIMIT}s"

# 3. 開始迴圈
# 使用 C-style for loop 以便逐一檢查
for ((L=START_L; L<=END_L; L++)); do
    
    # 跳過偶數
    if [ $((L % 2)) -eq 0 ]; then
        continue
    fi

    echo "========================================"
    echo "[Batch] Processing L=$L (Target: $TARGET_N, Max: ${TIME_LIMIT}s)"
    echo "========================================"

    # 執行 run.sh，並設定逾時
    # timeout ${TIME_LIMIT}s: 設定秒數限制
    timeout "${TIME_LIMIT}s" ./run.sh $L $TARGET_N

    # 取得狀態碼
    status=$?

    if [ $status -eq 124 ]; then
        # 124 = Timeout
        echo ""
        echo "----------------------------------------"
        echo "[Timeout] L=$L exceeded ${TIME_LIMIT}s. Skipping..."
        echo "----------------------------------------"
    elif [ $status -eq 0 ]; then
        # 0 = Success
        echo ""
        echo "----------------------------------------"
        echo "[Success] L=$L finished within time limit."
        echo "----------------------------------------"
    else
        # Other Errors
        echo ""
        echo "----------------------------------------"
        echo "[Error] L=$L failed with status $status."
        echo "----------------------------------------"
    fi

    # 緩衝
    sleep 1
done

echo ""
echo "[Batch] All tasks completed."