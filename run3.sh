#!/bin/bash

# =========================================================
# PACP Manager (Pure Execution Edition)
# Usage: ./run3.sh [Threads] [Start_L]
# =========================================================

NUM_WORKERS=${1:-8}
START_L=${2:-44} 

# 完整目標列表 (含一般與大型)
ALL_TARGETS=(
    44 46 58 68 86 90 94 
    110 112 140 152 154 156 174 184 186 188 190
)

# ---------------------------------------------------------
# 1. 產生待辦清單
# ---------------------------------------------------------
QUEUE=()
START_FLAG=0
for t in "${ALL_TARGETS[@]}"; do
    if [ "$t" -eq "$START_L" ]; then START_FLAG=1; fi
    if [ "$START_FLAG" -eq 1 ]; then QUEUE+=("$t"); fi
done

if [ ${#QUEUE[@]} -eq 0 ]; then
    echo "Invalid Start Length."
    exit 1
fi

# ---------------------------------------------------------
# 2. 檢查執行檔 (嚴格模式：只檢查，不編譯)
# ---------------------------------------------------------
C_RESET='\033[0m'
C_RED='\033[1;31m'
C_GREEN='\033[1;32m'
C_BLUE='\033[1;34m'
C_YELLOW='\033[1;33m'
C_CYAN='\033[1;36m'
C_WHITE='\033[1;37m'
C_GRAY='\033[0;90m'

echo -e "${C_BLUE}[Manager] Checking binary...${C_RESET}"

BINARY="./bin/optimizer3.exe"

# 檢查檔案是否存在
if [ ! -f "$BINARY" ]; then
    # 也許在 Linux 環境下沒有 .exe
    if [ -f "./bin/optimizer3" ]; then
        BINARY="./bin/optimizer3"
    else
        echo -e "${C_RED}[Error] Binary not found at: $BINARY${C_RESET}"
        echo -e "${C_YELLOW}Please run './compile.sh' first!${C_RESET}"
        exit 1
    fi
fi

echo -e "${C_GREEN}[OK] Ready to launch: $BINARY${C_RESET}"

# 資料夾設定
LOG_DIR="./logs_manager"
RESULTS_ROOT="./results"
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$RESULTS_ROOT"

# ---------------------------------------------------------
# 3. 核心控制函數
# ---------------------------------------------------------
kill_workers() {
    if command -v taskkill &> /dev/null; then
        taskkill //F //IM optimizer3.exe > /dev/null 2>&1
    else
        pkill -f "optimizer3" > /dev/null 2>&1
    fi
}

cleanup() {
    tput cnorm
    echo -e "\n${C_RED}[Manager] Aborting...${C_RESET}"
    kill_workers
    exit 0
}
trap cleanup SIGINT SIGTERM

draw_header() {
    local L=$1
    local t=$2
    local q_str=$3
    tput home
    echo -e "${C_BLUE}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RESET}"
    printf "${C_BLUE}┃ PACP Runner       ${C_WHITE}| Current L=%-3d| Time: %-4s | Q: %-14s ${C_BLUE}┃${C_RESET}\n" "$L" "${t}s" "${q_str:0:14}"
    echo -e "${C_BLUE}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
    printf "${C_BLUE}┃ ID   ┃ Iter     ┃ Restarts ┃ BadK ┃ Status   ┃ Activity          ┃${C_RESET}\n"
    echo -e "${C_BLUE}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
}

draw_footer() {
    echo -e "${C_BLUE}┣━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
    echo -e "${C_BLUE}┃ Recent Events                                                    ┃${C_RESET}"
    grep -h "\[EVENT\]" "$LOG_DIR"/*.log 2>/dev/null | tail -n 3 | while read line; do
         CLEAN_MSG=$(echo "$line" | sed 's/\[EVENT\] //')
         printf "${C_BLUE}┃${C_RESET} %-64s ${C_BLUE}┃${C_RESET}\n" "$CLEAN_MSG"
    done
    echo -e "${C_BLUE}┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${C_RESET}"
}

# ---------------------------------------------------------
# 4. 主執行迴圈
# ---------------------------------------------------------
tput civis
kill_workers

for L in "${QUEUE[@]}"; do
    TARGET_SAVE_DIR="${RESULTS_ROOT}/${L}_PQCP"
    mkdir -p "$TARGET_SAVE_DIR"
    rm -f "$LOG_DIR"/worker_${L}_*.log

    # 啟動 Workers
    for ((i=1; i<=NUM_WORKERS; i++)); do
        $BINARY $L "$TARGET_SAVE_DIR" $i > "$LOG_DIR/worker_${L}_${i}.log" 2>&1 &
    done

    START_TIME=$(date +%s)
    FOUND=0
    
    REMAINING_Q=""
    for q_item in "${QUEUE[@]}"; do
        if [ "$q_item" -gt "$L" ]; then REMAINING_Q+="$q_item "; fi
    done
    if [ -z "$REMAINING_Q" ]; then REMAINING_Q="Done"; fi

    clear

    while [ $FOUND -eq 0 ]; do
        NOW=$(date +%s)
        ELAPSED=$((NOW - START_TIME))
        draw_header $L $ELAPSED "$REMAINING_Q"
        
        for ((i=1; i<=NUM_WORKERS; i++)); do
            LOG_FILE="$LOG_DIR/worker_${L}_${i}.log"
            ITER="."
            RST="."
            BADK="."
            STATE="BOOT"
            ACT="-"
            COLOR=$C_GRAY

            if [ -f "$LOG_FILE" ]; then
                STAT_LINE=$(grep "\[STAT\]" "$LOG_FILE" | tail -n 1)
                if [ ! -z "$STAT_LINE" ]; then
                    ITER=$(echo "$STAT_LINE" | grep -o 'Iter=[0-9]*' | cut -d= -f2)
                    RST=$(echo "$STAT_LINE" | grep -o 'Restarts=[0-9]*' | cut -d= -f2)
                    BADK=$(echo "$STAT_LINE" | grep -o 'BadK=[0-9]*' | cut -d= -f2)
                    
                    if [ "$BADK" -le 2 ]; then
                        COLOR=$C_GREEN; STATE="VALLEY"; ACT="Converging"
                    elif [ "$BADK" -le 6 ]; then
                        COLOR=$C_YELLOW; STATE="SLOPE"; ACT="Descending"
                    else
                        COLOR=$C_RED; STATE="MTN"; ACT="Searching"
                    fi
                fi
                
                EVENT_LINE=$(grep "\[EVENT\]" "$LOG_FILE" | tail -n 1)
                if [ ! -z "$EVENT_LINE" ]; then
                     ACTION=$(echo "$EVENT_LINE" | grep -o 'Action=[A-Za-z]*' | cut -d= -f2)
                     if [ "$ACTION" == "KICK" ]; then ACT="${C_RED}KICKED${COLOR}";
                     elif [ "$ACTION" == "Dive" ]; then ACT="${C_GREEN}DIVING${COLOR}";
                     elif [ "$ACTION" == "VICTORY" ]; then ACT="${C_WHITE}★ WIN ★${COLOR}"; FOUND=1; fi
                fi
            fi
            printf "${C_BLUE}┃${C_RESET} ${COLOR}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-17s${C_RESET} ${C_BLUE}┃${C_RESET}\n" \
                   "#$i" "$ITER" "$RST" "$BADK" "$STATE" "$ACT"
        done
        
        draw_footer
        
        # 存檔檢查
        HIT_FILE=$(ls "$TARGET_SAVE_DIR"/L${L}_*.csv 2>/dev/null | head -n 1)
        if [ ! -z "$HIT_FILE" ] && [ -s "$HIT_FILE" ]; then
            FOUND=1
            echo -e "\n${C_GREEN}>>> MATCH FOUND for L=${L}! Moving to next... <<<${C_RESET}"
            echo -e "File saved: $HIT_FILE"
            break
        elif [ $FOUND -eq 1 ]; then
             echo -e "\n${C_GREEN}>>> Victory detected! Flushing file... <<<${C_RESET}"
             sleep 2
             break
        fi
        sleep 1
    done
    
    kill_workers
    wait 2>/dev/null
    sleep 3
done

tput cnorm
echo -e "\n${C_GREEN}=== ALL TASKS COMPLETED ===${C_RESET}"