#!/bin/bash

# =========================================================
# PACP Manager (Anti-Glitched UI Edition)
# 修復點：顏色代碼與寬度計算分離，防止框線偏移
# =========================================================

NUM_WORKERS=${1:-8}
START_L=${2:-44} 

ALL_TARGETS=(44 46 58 68 86 90 94 110 112 140 152 154 156 174 184 186 188 190)

# ---------------------------------------------------------
# 環境與顏色定義 (使用非變量長度格式)
# ---------------------------------------------------------
C_RESET='\033[0m'; C_RED='\033[1;31m'; C_GREEN='\033[1;32m'; C_BLUE='\033[1;34m'
C_YELLOW='\033[1;33m'; C_CYAN='\033[1;36m'; C_WHITE='\033[1;37m'; C_GRAY='\033[0;90m'

BINARY="./bin/optimizer3.exe"
[ ! -f "$BINARY" ] && BINARY="./bin/optimizer3"

LOG_DIR="./logs_manager"
RESULTS_ROOT="./results"
mkdir -p "$LOG_DIR" "$RESULTS_ROOT"

# ---------------------------------------------------------
# UI 繪製函數 (關鍵修復：分離顏色與長度)
# ---------------------------------------------------------
kill_workers() {
    taskkill //F //IM optimizer3.exe > /dev/null 2>&1 || pkill -f "optimizer3" > /dev/null 2>&1
}

cleanup() {
    echo -ne "\033[?25h" # 顯示游標
    kill_workers
    exit 0
}
trap cleanup SIGINT SIGTERM

draw_header() {
    local L=$1; local elapsed=$2; local q_str=$3
    echo -ne "\033[H" # 回到左上角
    echo -e "${C_BLUE}┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓${C_RESET}"
    # 分段組合：確保顏色不影響 printf 補位
    local info_part=$(printf " PACP Runner | L=%-3d | Time: %-4ss | Q: %-14s " "$L" "$elapsed" "${q_str:0:14}")
    echo -e "${C_BLUE}┃${C_WHITE}${info_part}${C_BLUE}┃${C_RESET}"
    echo -e "${C_BLUE}┣━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━┳━━━━━━┳━━━━━━━━━━┳━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
    echo -e "${C_BLUE}┃${C_WHITE} ID   ┃ Iter     ┃ Restarts ┃ BadK ┃ Status   ┃ Activity          ${C_BLUE}┃${C_RESET}"
    echo -e "${C_BLUE}┣━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━╋━━━━━━╋━━━━━━━━━━╋━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
}

draw_row() {
    local id=$1; local iter=$2; local rst=$3; local badk=$4; local state=$5; local act=$6; local color=$7
    # 這裡的 printf 只處理數值，外框由 echo 定位，防止偏移
    printf "${C_BLUE}┃${C_RESET} %-4s ${C_BLUE}┃${C_RESET} ${color}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${color}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} ${color}%-4s${C_RESET} ${C_BLUE}┃${C_RESET} ${color}%-8s${C_RESET} ${C_BLUE}┃${C_RESET} %-17s ${C_BLUE}┃${C_RESET}\n" \
           "$id" "$iter" "$rst" "$badk" "$state" "${act:0:17}"
}

draw_footer() {
    echo -e "${C_BLUE}┣━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━┻━━━━━━┻━━━━━━━━━━┻━━━━━━━━━━━━━━━━━━━┫${C_RESET}"
    echo -e "${C_BLUE}┃${C_WHITE} Recent Events                                                    ${C_BLUE}┃${C_RESET}"
    local count=0
    while read -r line && [ $count -lt 3 ]; do
        local msg=$(echo "$line" | sed 's/.*\[EVENT\] //')
        printf "${C_BLUE}┃${C_RESET} %-64s ${C_BLUE}┃${C_RESET}\n" "${msg:0:64}"
        ((count++))
    done < <(grep -h "\[EVENT\]" "$LOG_DIR"/*.log 2>/dev/null | tail -n 3)
    
    # 補足空行防止外框縮水
    for ((i=count; i<3; i++)); do
        printf "${C_BLUE}┃${C_RESET} %-64s ${C_BLUE}┃${C_RESET}\n" " "
    done
    echo -e "${C_BLUE}┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛${C_RESET}"
}

# ---------------------------------------------------------
# 主迴圈
# ---------------------------------------------------------
echo -ne "\033[?25l" # 隱藏游標
clear

for L in "${ALL_TARGETS[@]}"; do
    # 處理 Start_L 邏輯
    [ "$L" -lt "$START_L" ] && continue
    
    kill_workers
    TARGET_SAVE_DIR="${RESULTS_ROOT}/${L}_PQCP"
    mkdir -p "$TARGET_SAVE_DIR"
    rm -f "$LOG_DIR"/*.log

    for ((i=1; i<=NUM_WORKERS; i++)); do
        $BINARY $L "$TARGET_SAVE_DIR" $i > "$LOG_DIR/worker_${L}_${i}.log" 2>&1 &
    done

    START_TIME=$(date +%s)
    FOUND=0
    Q_STR=$(echo "${ALL_TARGETS[@]}" | sed "s/.*$L //")

    while [ $FOUND -eq 0 ]; do
        draw_header $L $(( $(date +%s) - START_TIME )) "${Q_STR:-Done}"
        
        for ((i=1; i<=NUM_WORKERS; i++)); do
            LOG_FILE="$LOG_DIR/worker_${L}_${i}.log"
            ITER="."; RST="."; BADK="."; STATE="BOOT"; ACT="-"; COLOR=$C_GRAY
            
            if [ -f "$LOG_FILE" ]; then
                STAT_LINE=$(grep "\[STAT\]" "$LOG_FILE" | tail -n 1)
                if [ -n "$STAT_LINE" ]; then
                    ITER=$(echo "$STAT_LINE" | grep -o 'Iter=[0-9]*' | cut -d= -f2)
                    RST=$(echo "$STAT_LINE" | grep -o 'Restarts=[0-9]*' | cut -d= -f2)
                    BADK=$(echo "$STAT_LINE" | grep -o 'BadK=[0-9]*' | cut -d= -f2)
                    [ "$BADK" -le 2 ] && { COLOR=$C_GREEN; STATE="VALLEY"; ACT="Converging"; } || \
                    [ "$BADK" -le 6 ] && { COLOR=$C_YELLOW; STATE="SLOPE"; ACT="Descending"; } || \
                    { COLOR=$C_RED; STATE="MTN"; ACT="Searching"; }
                fi
                grep -q "VICTORY" "$LOG_FILE" && { ACT="★ WIN ★"; COLOR=$C_WHITE; FOUND=1; }
            fi
            draw_row "#$i" "$ITER" "$RST" "$badk" "$STATE" "$ACT" "$COLOR"
        done
        draw_footer

        [ -n "$(ls "$TARGET_SAVE_DIR"/L${L}_*.csv 2>/dev/null)" ] && FOUND=1
        [ $FOUND -eq 1 ] && { echo -e "\n${C_GREEN}>>> SUCCESS: L=$L <<<${C_RESET}"; sleep 2; clear; break; }
        sleep 1
    done
done
cleanup