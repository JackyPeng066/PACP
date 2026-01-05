#!/bin/bash

# =========================================================
# PACP Manager v8.0 (Execution Only - No Compile)
# Usage: ./manager_v8.sh [Threads] [Start_L]
# Example: ./manager_v8.sh 12 46
# =========================================================

NUM_WORKERS=${1:-8}
START_L=${2:-44} 

# 完整目標列表
ALL_TARGETS=(44 46 58 68 86 90 94)

# ---------------------------------------------------------
# 1. 產生待辦清單 (Target Queue)
# ---------------------------------------------------------
QUEUE=()
START_FLAG=0

for t in "${ALL_TARGETS[@]}"; do
    if [ "$t" -eq "$START_L" ]; then START_FLAG=1; fi
    if [ "$START_FLAG" -eq 1 ]; then QUEUE+=("$t"); fi
done

if [ ${#QUEUE[@]} -eq 0 ]; then
    echo -e "\033[1;31m[Error] Invalid Start Length: $START_L\033[0m"
    echo "Available: ${ALL_TARGETS[*]}"
    exit 1
fi

# ---------------------------------------------------------
# 2. 環境檢查 (不編譯，只檢查檔案)
# ---------------------------------------------------------
# 顏色定義
C_RESET='\033[0m'
C_RED='\033[1;31m'
C_GREEN='\033[1;32m'
C_YELLOW='\033[1;33m'
C_BLUE='\033[1;34m'
C_CYAN='\033[1;36m'
C_WHITE='\033[1;37m'
C_GRAY='\033[0;90m'

echo -e "${C_BLUE}[Manager] Checking binary...${C_RESET}"

# 直接檢查執行檔是否存在
BINARY="./bin/optimizer3"

# Windows 相容性檢查 (.exe)
if [ -f "./bin/optimizer3.exe" ]; then
    BINARY="./bin/optimizer3.exe"
fi

if [ ! -f "$BINARY" ]; then
    echo -e "${C_RED}[Error] Binary not found at $BINARY${C_RESET}"
    echo -e "${C_YELLOW}Please compile manually using:${C_RESET}"
    echo "g++ -O3 -std=c++17 -march=native -o bin/optimizer3.exe src/optimizer3.cpp -static"
    exit 1
fi

echo -e "${C_GREEN}[OK] Found binary: $BINARY${C_RESET}"

# 資料夾設定 (相對路徑)
LOG_DIR="./logs_manager"
RESULTS_ROOT="./results"

# 清理舊 Log，保留 Results
rm -rf "$LOG_DIR"
mkdir -p "$LOG_DIR"
mkdir -p "$RESULTS_ROOT"

# ---------------------------------------------------------
# 3. 核心函數
# ---------------------------------------------------------

# 跨平台殺程序 (Windows/Linux)
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

# 繪製儀表板 Header
draw_header() {
    local L=$1
    local t=$2
    local q_str=$3
    
    tput home
    echo -e "${C_BLUE}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RESET}"
    printf "${C_BLUE}┃ PACP Manager v8   ${C_WHITE}| Current L=%-2d | Time: %-4s | Q: %-14s ${C_BLUE}┃${C_RESET}\n" "$L" "${t}s" "${q_str:0:14}"
    echo -e "${C_BLUE}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
    printf "${C_BLUE}┃ ID   ┃ Iter     ┃ Restarts ┃ BadK ┃ Status   ┃ Activity          ┃${C_RESET}\n"
    echo -e "${C_BLUE}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
}

# 繪製底部事件區
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
tput civis # 隱藏游標
kill_workers # 確保乾淨開局

for L in "${QUEUE[@]}"; do
    
    # 準備該長度的存檔目錄
    TARGET_SAVE_DIR="${RESULTS_ROOT}/${L}_PQCP"
    mkdir -p "$TARGET_SAVE_DIR"
    
    # 清除舊 Log
    rm -f "$LOG_DIR"/worker_${L}_*.log

    # 啟動 Workers
    for ((i=1; i<=NUM_WORKERS; i++)); do
        $BINARY $L "$TARGET_SAVE_DIR" $i > "$LOG_DIR/worker_${L}_${i}.log" 2>&1 &
    done

    START_TIME=$(date +%s)
    FOUND=0
    
    # 剩餘 Queue 字串化
    REMAINING_Q=""
    for q_item in "${QUEUE[@]}"; do
        if [ "$q_item" -gt "$L" ]; then REMAINING_Q+="$q_item "; fi
    done
    if [ -z "$REMAINING_Q" ]; then REMAINING_Q="Done"; fi

    # 清空畫面準備顯示新的 L
    clear

    while [ $FOUND -eq 0 ]; do
        NOW=$(date +%s)
        ELAPSED=$((NOW - START_TIME))
        
        # 1. 繪製 Header
        draw_header $L $ELAPSED "$REMAINING_Q"
        
        # 2. 更新 Worker 狀態
        for ((i=1; i<=NUM_WORKERS; i++)); do
            LOG_FILE="$LOG_DIR/worker_${L}_${i}.log"
            
            # 預設值 (對齊用)
            ITER="."
            RST="."
            BADK="."
            STATE="BOOT"
            ACT="-"
            COLOR=$C_GRAY

            if [ -f "$LOG_FILE" ]; then
                # 讀取 [STAT]
                STAT_LINE=$(grep "\[STAT\]" "$LOG_FILE" | tail -n 1)
                if [ ! -z "$STAT_LINE" ]; then
                    ITER=$(echo "$STAT_LINE" | grep -o 'Iter=[0-9]*' | cut -d= -f2)
                    RST=$(echo "$STAT_LINE" | grep -o 'Restarts=[0-9]*' | cut -d= -f2)
                    BADK=$(echo "$STAT_LINE" | grep -o 'BadK=[0-9]*' | cut -d= -f2)
                    
                    if [ "$BADK" -le 2 ]; then
                        COLOR=$C_GREEN
                        STATE="VALLEY"
                        ACT="Converging"
                    elif [ "$BADK" -le 6 ]; then
                        COLOR=$C_YELLOW
                        STATE="SLOPE"
                        ACT="Descending"
                    else
                        COLOR=$C_RED
                        STATE="MTN"
                        ACT="Searching"
                    fi
                fi
                
                # 讀取 [EVENT]
                EVENT_LINE=$(grep "\[EVENT\]" "$LOG_FILE" | tail -n 1)
                if [ ! -z "$EVENT_LINE" ]; then
                     ACTION=$(echo "$EVENT_LINE" | grep -o 'Action=[A-Za-z]*' | cut -d= -f2)
                     if [ "$ACTION" == "KICK" ]; then ACT="${C_RED}KICKED${COLOR}";
                     elif [ "$ACTION" == "Dive" ]; then ACT="${C_GREEN}DIVING${COLOR}";
                     elif [ "$ACTION" == "VICTORY" ]; then ACT="${C_WHITE}★ WIN ★${COLOR}"; FOUND=1; fi
                fi
            fi
            
            # 格式化輸出
            printf "${C_BLUE}┃${C_RESET} ${COLOR}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${COLOR}%-17s${C_RESET} ${C_BLUE}┃${C_RESET}\n" \
                   "#$i" "$ITER" "$RST" "$BADK" "$STATE" "$ACT"
        done
        
        # 3. 繪製 Footer
        draw_footer $NUM_WORKERS

        # 4. 存檔確認
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
    
    # 殺掉當前 Worker，準備下一個
    kill_workers
    wait 2>/dev/null
    sleep 3
done

tput cnorm
echo -e "\n${C_GREEN}===========================================${C_RESET}"
echo -e "${C_GREEN}   ALL TASKS COMPLETED SUCCESSFULLY!       ${C_RESET}"
echo -e "${C_GREEN}===========================================${C_RESET}"