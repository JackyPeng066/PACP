#!/bin/bash

# ============================================================
# PACP Batch Runner (Range 29-99, Odd only)
# Constraint: Find 1 pair per L
# Timeout: 60 seconds per L
# ============================================================

# 1. 預先編譯 (避免編譯時間算在第一個 L 的 60秒內)
echo "[Batch] Pre-compiling executables..."
make

# 2. 開始迴圈：從 29 到 99，每次加 2
for L in {29..99..2}; do
    echo "========================================"
    echo "[Batch] Processing L=$L (Target: 1 pair, Max Time: 60s)"
    echo "========================================"

    # 執行 run.sh，並設定 60 秒逾時
    # timeout 60s: 設定 60 秒限制
    # ./run.sh $L 1: 呼叫你的主腳本，找 1 組
    timeout 60s ./run.sh $L 1

    # 取得上一行指令的離開狀態碼 (Exit Status)
    status=$?

    if [ $status -eq 124 ]; then
        # 狀態碼 124 代表被 timeout 強制終止
        echo ""
        echo "----------------------------------------"
        echo "[Timeout] L=$L exceeded 60 seconds. Skipping to next..."
        echo "----------------------------------------"
    elif [ $status -eq 0 ]; then
        # 狀態碼 0 代表成功執行完畢
        echo ""
        echo "----------------------------------------"
        echo "[Success] L=$L finished within time limit."
        echo "----------------------------------------"
    else
        # 其他錯誤
        echo ""
        echo "----------------------------------------"
        echo "[Error] L=$L failed with status $status."
        echo "----------------------------------------"
    fi

    # 暫停 1 秒讓 I/O 緩衝區寫入，避免 Log 錯亂
    sleep 1
done

echo ""
echo "[Batch] All tasks (29-99) completed."