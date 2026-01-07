#!/bin/bash

# =========================================================
# PACP Manager v38.0 (Stable Dashboard)
# Usage: ./run_pqcp.sh <NumWorkers> <Length>
# =========================================================

if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: ./run_pqcp.sh <n> <L>"
    exit 1
fi

NUM_WORKERS=$1
TARGET_L=$2

RESULTS_ROOT="results-c/results by computer/N96141066-T2-4"
TARGET_MAIN_DIR="${RESULTS_ROOT}/${TARGET_L}_PACP"
SOL_FILE="${TARGET_MAIN_DIR}/${TARGET_L}_PQCP.txt"
NEAR_FILE="${TARGET_MAIN_DIR}/${TARGET_L}_near.txt"
LOG_DIR="./logs_pqcp"
BIN_DIR="./bin"
BINARY_NAME="optimizer_pqcp"

if [ -f "${BIN_DIR}/${BINARY_NAME}.exe" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}.exe";
elif [ -f "${BIN_DIR}/${BINARY_NAME}" ]; then BINARY="${BIN_DIR}/${BINARY_NAME}";
else echo "Error: Binary not found!"; exit 1; fi

mkdir -p "$LOG_DIR" "$RESULTS_ROOT"
rm -rf "${TARGET_MAIN_DIR}/Workers" 

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
    echo -e "\n\033[1;31m[System] Stopped.\033[0m"
    exit 0
}
trap cleanup SIGINT SIGTERM

kill_workers
rm -f "$LOG_DIR"/*.log
echo -e "\033[1;33m>>> Starting $NUM_WORKERS workers for L=$TARGET_L (Stable Mode)...\033[0m"

for ((i=1; i<=NUM_WORKERS; i++)); do
    "$BINARY" "$TARGET_L" "$RESULTS_ROOT" "$i" > "$LOG_DIR/worker_${i}.log" 2>&1 &
done

echo -ne "\033[?25h" 
clear
START_TIME=$(date +%s)

C_RST='\033[0m'; C_GRN='\033[1;32m'; C_BLU='\033[1;34m'; C_WHT='\033[1;37m'
C_YEL='\033[1;33m'; C_RED='\033[1;31m'; C_GRY='\033[0;90m'

while true; do
    ELAPSED=$(( $(date +%s) - START_TIME ))
    
    PQCP_COUNT=0
    NEAR_COUNT=0
    if [ -f "$SOL_FILE" ]; then PQCP_COUNT=$(grep -c "PQCP" "$SOL_FILE"); fi
    if [ -f "$NEAR_FILE" ]; then NEAR_COUNT=$(grep -c "NEAR" "$NEAR_FILE"); fi

    echo -ne "\033[H"
    echo -e "${C_BLU}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RST}\033[K"
    printf "${C_BLU}┃${C_WHT} PQCP Hunter | L=%-3d | Time: %-4ss | ${C_GRN}PQCP: %-3d${C_WHT} | ${C_YEL}Near: %-3d${C_BLU} ┃${C_RST}\033[K\n" \
           "$TARGET_L" "$ELAPSED" "$PQCP_COUNT" "$NEAR_COUNT"
    echo -e "${C_BLU}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━┳━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"
    echo -e "${C_BLU}┃${C_WHT} ID   ┃ Iter     ┃ Restarts ┃ Viol ┃ Peak ┃ Status            ${C_BLU}┃${C_RST}\033[K"
    echo -e "${C_BLU}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━╋━━━━━━━━━━━━━━━━━━━┫${C_RST}\033[K"

    for ((i=1; i<=NUM_WORKERS; i++)); do
        STATUS_F="${TARGET_MAIN_DIR}/Workers/Worker_${i}/status.txt"
        
        ITER="-"; RST="-"; VIOL="-"; PEAKS="-"; STATE="BOOT"; COLOR=$C_GRY
        
        if [ -f "$STATUS_F" ]; then
            # 讀取並過濾非法字符
            read -r LINE < "$STATUS_F"
            if [ -n "$LINE" ]; then
                ITER=$(echo "$LINE" | awk -F'Iter=' '{print $2}' | awk '{print $1}')
                RST=$(echo "$LINE"  | awk -F'Rst='  '{print $2}' | awk '{print $1}')
                VIOL=$(echo "$LINE" | awk -F'Viol=' '{print $2}' | awk '{print $1}')
                PEAKS=$(echo "$LINE"| awk -F'Peaks=' '{print $2}'| awk '{print $1}')
                
                # [關鍵修正] 檢查變數是否為純數字，否則略過判斷
                if [[ "$VIOL" =~ ^[0-9]+$ ]] && [[ "$PEAKS" =~ ^[0-9]+$ ]]; then
                    if [ "$VIOL" -eq 0 ]; then 
                        if [ "$PEAKS" -eq 2 ]; then COLOR=$C_WHT; STATE="★ HIT ★";
                        elif [ "$PEAKS" -le 6 ]; then COLOR=$C_YEL; STATE="NEAR HIT ($PEAKS)";
                        else COLOR=$C_GRN; STATE="SHAPING"; fi
                    elif [ "$VIOL" -le 4 ]; then COLOR=$C_YEL; STATE="CONVERGING";
                    else COLOR=$C_RED; STATE="SEARCHING"; fi
                else
                    STATE="READING..." # 資料不完整時的暫態
                fi
            fi
        fi
        
        printf "${C_BLU}┃${C_RST} #%-3d  ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${C_GRY}%-8s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-4s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-4s${C_RST} ${C_BLU}┃${C_RST} ${COLOR}%-17s${C_RST} ${C_BLU}┃${C_RST}\033[K\n" \
               "$i" "${ITER:0:8}" "${RST:0:6}" "${VIOL:0:4}" "${PEAKS:0:4}" "$STATE"
    done
    echo -e "${C_BLU}┗━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━┻━━━━━━━━━━━━━━━━━━━┛${C_RST}\033[K"
    
    echo -ne "\033[J"
    sleep 0.5
done