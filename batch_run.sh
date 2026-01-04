#!/bin/bash

# ============================================================
# PACP Batch Runner v2.0 (Smart Range)
# Logic:
#   1. Start=Odd, End=Odd   -> Run ODD only.
#   2. Start=Even, End=Even -> Run EVEN only (Opt-PACP check).
#   3. Mixed Parity         -> Run ALL (Smart Filter).
# ============================================================

# --- 顏色定義 ---
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
GRAY='\033[1;30m'
NC='\033[0m'

# 1. 參數檢查
if [ "$#" -lt 4 ]; then
    echo -e "${YELLOW}Usage: ./batch_run.sh <Start_L> <End_L> <Count> <Timeout>${NC}"
    echo "--------------------------------------------------------"
    echo " Logic Examples:"
    echo "  ./batch_run.sh 29 33 ... -> Runs 29, 31, 33 (Odd Only)"
    echo "  ./batch_run.sh 28 32 ... -> Runs 28, 30, 32 (Even Only)"
    echo "  ./batch_run.sh 28 33 ... -> Runs 28..33 (All)"
    echo "--------------------------------------------------------"
    exit 1
fi

START_L=$1
END_L=$2
TARGET_N=$3
TIME_LIMIT=$4
LOG_FILE="batch_report.log"

if [ "$START_L" -gt "$END_L" ]; then
    echo -e "${RED}[Error] Start_L ($START_L) cannot be greater than End_L ($END_L).${NC}"
    exit 1
fi

# --- 2. 決定執行模式 (Mode Detection) ---
START_IS_ODD=$((START_L % 2))
END_IS_ODD=$((END_L % 2))
BATCH_MODE=""

if [ "$START_IS_ODD" -ne 0 ] && [ "$END_IS_ODD" -ne 0 ]; then
    BATCH_MODE="ODD_ONLY"
    MODE_DESC="Odd L only (Skipping Evens)"
elif [ "$START_IS_ODD" -eq 0 ] && [ "$END_IS_ODD" -eq 0 ]; then
    BATCH_MODE="EVEN_ONLY"
    MODE_DESC="Even L only (Skipping Odds)"
else
    BATCH_MODE="MIXED"
    MODE_DESC="All L (Mixed)"
fi

# --- Goal 2 白名單 (Opt-PACP) ---
GOAL2_LIST=" 6 12 14 22 24 28 30 38 42 48 54 56 60 62 66 70 76 78 84 88 92 96 102 108 114 118 120 124 126 132 134 138 142 150 158 166 168 172 176 182 192 198 "

# --- 中斷保護 ---
trap "echo -e '\n${RED}[!] Batch Interrupted. Killing all processes...${NC}'; kill 0; exit 1" SIGINT SIGTERM

# --- 編譯 ---
echo -e "${BLUE}[Batch] Pre-compiling...${NC}"
make > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "${RED}[Error] Make failed.${NC}"
    exit 1
fi

# --- 統計變數 ---
TOTAL_COUNT=0
SKIP_COUNT=0
RUN_COUNT=0
SUCCESS_COUNT=0
TIMEOUT_COUNT=0

echo "========================================================"
echo -e " Batch Range: ${START_L} -> ${END_L}"
echo -e " Mode Detected: ${YELLOW}${BATCH_MODE}${NC} ($MODE_DESC)"
echo -e " Timeout: ${TIME_LIMIT}s | Target: ${TARGET_N}"
echo "========================================================"
echo "--- Batch Run: $(date) | Range: $START_L-$END_L | Mode: $BATCH_MODE ---" >> "$LOG_FILE"

# --- 3. 開始迴圈 ---
for ((L=START_L; L<=END_L; L++)); do
    TOTAL_COUNT=$((TOTAL_COUNT + 1))
    
    IS_ODD=$((L % 2))
    SHOULD_RUN=0
    MSG_TYPE=""
    SKIP_REASON=""

    # === [智慧過濾核心] ===
    
    if [ "$IS_ODD" -ne 0 ]; then
        # --- 目前是奇數 ---
        if [ "$BATCH_MODE" == "EVEN_ONLY" ]; then
            SHOULD_RUN=0
            SKIP_REASON="Mode is Even Only"
        else
            # 奇數總是執行 (Goal 1)
            SHOULD_RUN=1
            MSG_TYPE="Goal 1 (Odd)"
        fi
    else
        # --- 目前是偶數 ---
        if [ "$BATCH_MODE" == "ODD_ONLY" ]; then
            SHOULD_RUN=0
            SKIP_REASON="Mode is Odd Only"
        else
            # 檢查白名單
            if [[ "$GOAL2_LIST" =~ " $L " ]]; then
                SHOULD_RUN=1
                MSG_TYPE="Goal 2 (Opt-PACP)"
            else
                SHOULD_RUN=0
                SKIP_REASON="PCP Candidate (Not in Target List)"
            fi
        fi
    fi

    # === [執行或跳過] ===
    if [ "$SHOULD_RUN" -eq 0 ]; then
        # 僅在非大量跳過時顯示，或者簡化顯示
        # 這裡選擇簡化顯示跳過訊息
        # echo -e "${GRAY}[Skip] L=$L ($SKIP_REASON)${NC}"
        SKIP_COUNT=$((SKIP_COUNT + 1))
        continue
    fi

    RUN_COUNT=$((RUN_COUNT + 1))
    
    echo -e "--------------------------------------------------------"
    TIME_STR="Max: ${TIME_LIMIT}s"
    [ "$TIME_LIMIT" -eq 0 ] && TIME_STR="Infinite"
    
    echo -e "${BLUE}[Running] L=$L${NC} | Type: $MSG_TYPE | $TIME_STR"

    CMD="./run.sh $L $TARGET_N"
    START_TIME=$(date +%s)
    
    if [ "$TIME_LIMIT" -eq 0 ]; then
        $CMD
        RET_CODE=$?
    else
        timeout "${TIME_LIMIT}s" $CMD
        RET_CODE=$?
    fi
    
    END_TIME=$(date +%s)
    ELAPSED=$((END_TIME - START_TIME))

    if [ $RET_CODE -eq 124 ]; then
        echo -e "   -> ${YELLOW}[Timeout] L=$L stopped after ${ELAPSED}s.${NC}"
        echo "L=$L : Timeout" >> "$LOG_FILE"
        TIMEOUT_COUNT=$((TIMEOUT_COUNT + 1))
    elif [ $RET_CODE -eq 0 ]; then
        echo -e "   -> ${GREEN}[Success] L=$L finished in ${ELAPSED}s.${NC}"
        echo "L=$L : Success (${ELAPSED}s)" >> "$LOG_FILE"
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    else
        echo -e "   -> ${RED}[Error] L=$L failed with status $RET_CODE.${NC}"
        echo "L=$L : Error ($RET_CODE)" >> "$LOG_FILE"
    fi

    sleep 1
done

# --- 最終統計 ---
echo ""
echo "========================================================"
echo -e "           ${BLUE}BATCH SUMMARY (${BATCH_MODE})${NC}"
echo "========================================================"
echo " Range       : $START_L to $END_L"
echo " Skipped     : $SKIP_COUNT"
echo " Executed    : $RUN_COUNT"
echo "--------------------------------------------------------"
echo -e " ${GREEN}Success     : $SUCCESS_COUNT${NC}"
echo -e " ${YELLOW}Timeout     : $TIMEOUT_COUNT${NC}"
echo "========================================================"

exit 0