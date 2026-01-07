#!/bin/bash

# =========================================================
# PQCP Sequential Hunter (Auto-Advance)
# List: 44 46 58 68 86 90 94 110 112 140 152 154 156 174 184 186 188 190
# =========================================================

# 1. 檢查參數
if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: ./run_seq.sh <NumWorkers> <Start_L>"
    echo "Example: ./run_seq.sh 20 46"
    exit 1
fi

NUM_WORKERS=$1
START_L=$2

# 2. 定義目標列表
ALL_TARGETS=(44 46 58 68 86 90 94 110 112 140 152 154 156 174 184 186 188 190)

# 基礎設定
RESULTS_ROOT="results-c/results by computer/N96141066-T2-4"
LOG_DIR="./logs_pqcp"
BIN_DIR="./bin"
BINARY_NAME="optimizer_pqcp"

# 檢查執行檔
if [ -f "${BIN_DIR}/${BINARY_NAME}.exe" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}.exe";
elif [ -f "${BIN_DIR}/${BINARY_NAME}" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}";
else echo "Error: Binary not found in $BIN_DIR !"; exit 1; fi

mkdir -p "$LOG_DIR" "$RESULTS_ROOT"

# --- 輔助函數 ---
kill_workers() {
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        taskkill //F //IM "${BINARY_NAME}.exe" > /dev/null 2>&1
    else
        pkill -f "${BINARY_NAME}" > /dev/null 2>&1
    fi
}

cleanup() {
    echo -ne "\033[?25h"
    kill_workers
    echo -e "\n\033[1;31m[System] User Aborted. Stopped.\033[0m"
    exit 0
}
trap cleanup SIGINT SIGTERM

# --- 主邏輯 ---

FOUND_START=false

for TARGET_L in "${ALL_TARGETS[@]}"; do
    # 1. 跳過起始長度之前的目標
    if [ "$FOUND_START" = false ]; then
        if [ "$TARGET_L" -eq "$START_L" ]; then
            FOUND_START=true
        else
            continue
        fi
    fi

    # 2. 準備環境
    TARGET_MAIN_DIR="${RESULTS_ROOT}/${TARGET_L}_PACP"
    SOL_FILE="${TARGET_MAIN_DIR}/${TARGET_L}_PQCP.txt"
    NEAR_FILE="${TARGET_MAIN_DIR}/${TARGET_L}_near.txt"
    
    # 清理舊狀態 (確保儀表板乾淨)
    rm -rf "${TARGET_MAIN_DIR}/Workers"
    rm -f "$LOG_DIR"/*.log

    kill_workers
    echo -e "\n\033[1;33m>>> [NEW TARGET] Launching L=$TARGET_L with $NUM_WORKERS workers...\033[0m"
    sleep 2

    # 3. 啟動 Workers
    for ((i=1; i<=NUM_WORKERS; i++)); do
        "$BINARY" "$TARGET_L" "$RESULTS_ROOT" "$i" > "$LOG_DIR/worker_${i}.log" 2>&1 &
    done

    # 4. 監控與儀表板迴圈
    echo -ne "\033[?25h" 
    START_TIME=$(date +%s)
    
    # Colors
    C_RST='\033[0m'; C_GRN='\033[1;32m'; C_BLU='\033[1;34m'; C_WHT='\033[1;37m'
    C_YEL='\033[1;33m'; C_RED='\033[1;31m'; C_GRY='\033[0;90m'

    TARGET_FOUND=false

    while [ "$TARGET_FOUND" = false ]; do
        ELAPSED=$(( $(date +%s) - START_TIME ))
        
        # 檢查是否找到解
        PQCP_COUNT=0
        if [ -f "$SOL_FILE" ]; then 
            PQCP_COUNT=$(grep -c "PQCP" "$SOL_FILE")
        fi

        # 如果找到至少一個 PQCP，退出迴圈
        if [ "$PQCP_COUNT" -gt 0 ]; then
            TARGET_FOUND=true
            break
        fi

        # 讀取 Near Count
        NEAR_COUNT=0
        if [ -f "$NEAR_FILE" ]; then NEAR_COUNT=$(grep -c "NEAR" "$NEAR_FILE"); fi

        # 繪製儀表板
        echo -ne "\033[H"
        echo -e "${C_BLU}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RST}\033[K"
        printf "${C_BLU}┃${C_WHT} PQCP Auto-Seq | L=%-3d | Time: %-4ss | ${C_GRN}PQCP: %-3d${C_WHT} | ${C_YEL}Near: %-3d${C_BLU} ┃${C_RST}\033[K\n" \
               "$TARGET_L" "$ELAPSED" "$PQCP_COUNT" "$NEAR_COUNT"
        echo -e "${C_BLU}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━┳━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"
        echo -e "${C_BLU}┃${C_WHT} ID   ┃ Iter     ┃ Restarts ┃ Viol ┃ Peak ┃ Status            ${C_BLU}┃${C_RST}\033[K"
        echo -e "${C_BLU}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━╋━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"

        # 顯示前 10 個 Worker 的狀態 (避免螢幕太長)
        SHOW_LIMIT=$NUM_WORKERS
        if [ "$SHOW_LIMIT" -gt 10 ]; then SHOW_LIMIT=10; fi

        for ((i=1; i<=SHOW_LIMIT; i++)); do
            STATUS_F="${TARGET_MAIN_DIR}/Workers/Worker_${i}/status.txt"
            ITER="-"; RST="-"; VIOL="-"; PEAKS="-"; STATE="BOOT"; COLOR=$C_GRY
            
            if [ -f "$STATUS_F" ]; then
                read -r LINE < "$STATUS_F"
                if [ -n "$LINE" ]; then
                    ITER=$(echo "$LINE" | awk -F'Iter=' '{print $2}' | awk '{print $1}')
                    RST=$(echo "$LINE"  | awk -F'Rst='  '{print $2}' | awk '{print $1}')
                    VIOL=$(echo "$LINE" | awk -F'Viol=' '{print $2}' | awk '{print $1}')
                    PEAKS=$(echo "$LINE"| awk -F'Peaks=' '{print $2}'| awk '{print $1}')
                    
                    if [[ "$VIOL" =~ ^[0-9]+$ ]] && [[ "$PEAKS" =~ ^[0-9]+$ ]]; then
                        if [ "$VIOL" -eq 0 ]; then 
                            if [ "$PEAKS" -eq 2 ]; then COLOR=$C_WHT; STATE="★ HIT ★";
                            elif [ "$PEAKS" -le 6 ]; then COLOR=$C_YEL; STATE="NEAR HIT ($PEAKS)";
                            else COLOR=$C_GRN; STATE="SHAPING"; fi
                        elif [ "$VIOL" -le 4 ]; then COLOR=$C_YEL; STATE="CONVERGING";
                        else COLOR=$C_RED; STATE="SEARCHING"; fi
                    fi
                fi
            fi
            printf "${C_BLU}┃${C_RST} #%-3d  ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-4s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-4s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-17s${C_RST} ${C_BLU}┃${C_RST}\033[K\n" \
                   "$i" "${ITER:0:8}" "${RST:0:6}" "${VIOL:0:4}" "${PEAKS:0:4}" "$STATE"
        done
        echo -e "${C_BLU}┗━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━┻━━━━━━━━━━━━━━━━━━━┛${C_RST}\033[K"
        if [ "$NUM_WORKERS" -gt 10 ]; then echo -e "  ... (Hiding $((NUM_WORKERS-10)) workers) ..."; fi
        
        sleep 1
    done

    # 5. 找到解後的處理
    kill_workers
    echo -e "\n\033[1;32m[SUCCESS] Found PQCP for L=$TARGET_L in ${ELAPSED}s!\033[0m"
    echo -e "Saved to: $SOL_FILE"
    echo -e "Moving to next target in 5 seconds..."
    sleep 5
    clear

done

echo -e "\n\033[1;32m[COMPLETE] All targets finished!\033[0m"