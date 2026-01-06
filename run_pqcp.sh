#!/bin/bash

# =========================================================
# PACP Manager V23.3 (Visual Cursor Edition)
# Usage: ./run_pqcp.sh <NumWorkers> <Length>
# =========================================================

# 1. 參數檢查
if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: ./run_pqcp.sh <n> <L>"
    exit 1
fi

NUM_WORKERS=$1
TARGET_L=$2

# 2. 設定路徑
RESULTS_ROOT="results-c/results by computer/N96141066-T2-4"
LOG_DIR="./logs_pqcp"
BIN_DIR="./bin"
BINARY_NAME="optimizer_pqcp"

# 尋找執行檔
if [ -f "${BIN_DIR}/${BINARY_NAME}.exe" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}.exe";
elif [ -f "${BIN_DIR}/${BINARY_NAME}" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}";
else echo "Error: Binary not found!"; exit 1; fi

mkdir -p "$LOG_DIR" "$RESULTS_ROOT"

# 目標檔案
TARGET_SUBDIR="${RESULTS_ROOT}/${TARGET_L}_PACP"
SOL_FILE="${TARGET_SUBDIR}/${TARGET_L}_solutions.txt"

# 記錄初始行數
if [ -f "$SOL_FILE" ]; then
    INIT_COUNT=$(grep -c . "$SOL_FILE")
else
    INIT_COUNT=0
fi

# ---------------------------------------------------------
# 3. 啟動與清理
# ---------------------------------------------------------
kill_workers() {
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        taskkill //F //IM "${BINARY_NAME}.exe" > /dev/null 2>&1
    else
        pkill -f "${BINARY_NAME}" > /dev/null 2>&1
    fi
}

cleanup() {
    echo -ne "\033[?25h" # 確保結束時游標可見
    kill_workers
    echo -e "\n\033[1;31m[System] Stopped.\033[0m"
    exit 0
}
trap cleanup SIGINT SIGTERM

kill_workers
rm -f "$LOG_DIR"/*.log
echo -e "\033[1;33m>>> Starting $NUM_WORKERS workers for L=$TARGET_L...\033[0m"

for ((i=1; i<=NUM_WORKERS; i++)); do
    "$BINARY" "$TARGET_L" "$RESULTS_ROOT" "$i" > "$LOG_DIR/worker_${i}.log" 2>&1 &
done

# ---------------------------------------------------------
# 4. 即時儀表板 (視覺化更新)
# ---------------------------------------------------------
# [關鍵調整] 顯式開啟游標，讓您看到更新過程
echo -ne "\033[?25h" 
clear
START_TIME=$(date +%s)

# Colors
C_RST='\033[0m'; C_GRN='\033[1;32m'; C_BLU='\033[1;34m'; C_WHT='\033[1;37m'
C_YEL='\033[1;33m'; C_RED='\033[1;31m'; C_GRY='\033[0;90m'

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))

    # 檢查總數
    if [ -f "$SOL_FILE" ]; then CURR_COUNT=$(grep -c . "$SOL_FILE"); else CURR_COUNT=0; fi
    NEW_FOUND=$((CURR_COUNT - INIT_COUNT))

    # [視覺核心] 游標歸位左上角 (不清除整個螢幕，避免閃爍)
    echo -ne "\033[H"
    
    echo -e "${C_BLU}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RST}\033[K"
    printf "${C_BLU}┃${C_WHT} PQCP Hunter | L=%-3d | Time: %-4ss | ${C_GRN}New: %-3d${C_WHT} (Tot:%-3d)   ${C_BLU}┃${C_RST}\033[K\n" \
           "$TARGET_L" "$ELAPSED" "$NEW_FOUND" "$CURR_COUNT"
    echo -e "${C_BLU}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"
    echo -e "${C_BLU}┃${C_WHT} ID   ┃ Iter     ┃ Restarts ┃ Viol ┃ Status                     ${C_BLU}┃${C_RST}\033[K"
    echo -e "${C_BLU}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"

    # Workers Loop
    for ((i=1; i<=NUM_WORKERS; i++)); do
        LOG="$LOG_DIR/worker_${i}.log"
        ITER="-"; RST="-"; VIOL="-"; STATE="BOOT"; COLOR=$C_GRY
        
        if [ -f "$LOG" ]; then
            if grep -q "BOOT" "$LOG"; then STATE="BOOTING"; fi
            
            # Robust tail reading
            RAW=$(tail -c 200 "$LOG" | tr '\r' '\n' | grep "\[STAT\]" | tail -n 1)
            
            if [ -n "$RAW" ]; then
                ITER=$(echo "$RAW" | grep -o 'Iter=[0-9]*' | cut -d= -f2)
                RST=$(echo "$RAW" | grep -o 'Rst=[0-9]*' | cut -d= -f2)
                VIOL=$(echo "$RAW" | grep -o 'Viol=[0-9]*' | cut -d= -f2)
                
                V_VAL=${VIOL:-99}
                if [ "$V_VAL" == "0" ]; then COLOR=$C_GRN; STATE="POLISHING";
                elif [ "$V_VAL" -le 4 ]; then COLOR=$C_YEL; STATE="CONVERGING";
                else COLOR=$C_RED; STATE="SEARCHING"; fi
            fi
            
            if grep -q "NEW_SOL" "$LOG"; then STATE="★ FOUND ★"; COLOR=$C_WHT; fi
        fi
        
        # \033[K 確保該行右側髒資料被清除
        printf "${C_BLU}┃${C_RST} #%-3d  ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-4s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-26s${C_RST} ${C_BLU}┃${C_RST}\033[K\n" \
               "$i" "${ITER:0:8}" "${RST:0:6}" "${VIOL:0:4}" "$STATE"
    done
    echo -e "${C_BLU}┗━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${C_RST}\033[K"

    # Show latest result
    if [ "$NEW_FOUND" -gt 0 ] && [ -f "$SOL_FILE" ]; then
        echo -e "\n${C_GRN}>>> LATEST HIT <<<${C_RST}\033[K"
        tail -n 1 "$SOL_FILE" | cut -c 1-80
        echo -ne "\033[K" # Clear extra line
    fi
    
    # 清除螢幕剩餘部分 (防止殘留)
    echo -ne "\033[J"
    
    sleep 0.2
done