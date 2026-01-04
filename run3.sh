#!/bin/bash

# =========================================================
# PACP Goal 3 Multi-Core Manager
# Usage: ./run3.sh [Threads]
# Example: ./run3.sh 14  (Runs 14 instances in parallel)
# =========================================================

# 預設執行緒數 (若沒輸入參數，預設 4)
NUM_WORKERS=${1:-4}

# 目標 L 列表
TARGETS=(44 46 58 68 86 90 94)

# 路徑設定
MASTER_FILE="results/goal3_final.csv"
POOL_DIR="results/goal3_pool"

# 顏色
GREEN='\033[1;32m'
BLUE='\033[1;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

# 初始化
mkdir -p "$POOL_DIR"
if [ ! -f "$MASTER_FILE" ]; then
    echo "L,PSL,A,B" > "$MASTER_FILE"
fi

# 清理舊的殘留程序 (防呆)
cleanup() {
    echo -e "\n${RED}[Manager] Stopping all workers...${NC}"
    pkill -P $$ optimizer3 2>/dev/null
    exit 0
}
trap cleanup SIGINT SIGTERM

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}   PACP Goal 3 Parallel Runner (x${NUM_WORKERS} Cores)   ${NC}"
echo -e "${BLUE}==================================================${NC}"

# 主迴圈
for L in "${TARGETS[@]}"; do
    
    # 1. 檢查是否已經解出
    if grep -q "^$L,4," "$MASTER_FILE"; then
        echo -e "${GREEN}[Done] L=$L already found. Skipping.${NC}"
        continue
    fi

    echo -e "\n${YELLOW}>>> Target L=${L} | Launching ${NUM_WORKERS} workers...${NC}"
    
    # 清空暫存池 (避免讀到上一次的垃圾)
    rm -f "$POOL_DIR"/L${L}_*.csv

    # 2. 啟動平行運算 (Workers)
    # 我們混合使用兩種模式來增加機率：
    # 一半跑對稱模式 (Sym=1)，一半跑暴力模式 (Sym=0)
    HALF_WORKERS=$((NUM_WORKERS / 2))
    
    for ((i=1; i<=NUM_WORKERS; i++)); do
        MODE=0
        if [ "$i" -le "$HALF_WORKERS" ]; then MODE=1; fi
        
        # 背景執行 (&)
        ./bin/optimizer3 $L "$POOL_DIR" $MODE > /dev/null 2>&1 &
    done

    # 3. 監控迴圈 (Monitor)
    START_TIME=$(date +%s)
    FOUND=0
    
    while [ $FOUND -eq 0 ]; do
        # 每秒檢查一次 POOL 資料夾有沒有新檔案
        COUNT=$(ls "$POOL_DIR"/L${L}_*.csv 2>/dev/null | wc -l)
        
        if [ "$COUNT" -gt 0 ]; then
            FOUND=1
            
            # 抓出第一個找到的檔案
            HIT_FILE=$(ls "$POOL_DIR"/L${L}_*.csv | head -n 1)
            
            echo -e "\n${GREEN}[SUCCESS] Worker found solution for L=${L}!${NC}"
            
            # 將結果 append 到主檔案
            cat "$HIT_FILE" >> "$MASTER_FILE"
            echo -e "   -> Saved to ${MASTER_FILE}"
            
            # 顯示結果
            cat "$HIT_FILE"
        fi

        # 顯示進度動畫
        ELAPSED=$(($(date +%s) - START_TIME))
        echo -ne "\r[Running] L=${L} | Time: ${ELAPSED}s | Workers active... "
        sleep 1
    done

    # 4. 殺掉所有背景 Worker (進入下一關前清場)
    # pkill -P $$ 會殺掉當前 script (父行程) 產生的所有子行程
    pkill -P $$ optimizer3
    wait 2>/dev/null # 確保殭屍行程回收
    
    echo -e "${BLUE}[Manager] Workers cleaned up. Moving next...${NC}"

done

echo -e "\n${GREEN}==================================================${NC}"
echo -e "${GREEN}   All Goal 3 Targets Completed!   ${NC}"
echo -e "${GREEN}==================================================${NC}"