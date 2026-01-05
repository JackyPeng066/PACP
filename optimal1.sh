#!/bin/bash

# =========================================================
# optimal1 v3.1 - 完善全量篩選版 (流式處理 + 成功清單)
# 修正：突破 1003 筆限制，並在報表頂端歸納成功長度
# =========================================================

# 1. 設置路徑
SRC_GOAL="results-g/Goal1"
DST_ROOT="results-final"
DST_GOAL="$DST_ROOT/Goal1"
REPORT="$DST_ROOT/optimal1_summary.txt"
TARGET_PSL=2

echo -e "\033[1;34m>>> 啟動 optimal1 完善版程序 (目標 PSL: $TARGET_PSL) <<<\033[0m"

# 確保目標根目錄存在
mkdir -p "$DST_GOAL"

# 2. 獲取 L 清單
ALL_L=$(ls -1 "$SRC_GOAL" 2>/dev/null | grep -E "^[0-9]+$" | sort -n)
TOTAL_L=$(echo "$ALL_L" | wc -w)

if [ "$TOTAL_L" -eq 0 ]; then
    echo -e "\033[1;31m❌ 錯誤：在 $SRC_GOAL 中找不到資料夾！\033[0m"
    exit 1
fi

# 初始化暫存變數與檔案
COUNTER=0
MATCH_TOTAL=0
SUCCESS_L_LIST=""
TEMP_FILE=".optimal_temp_$(date +%s)"
TEMP_REPORT=".report_temp"

# 3. 執行循環處理
for L in $ALL_L; do
    ((COUNTER++))
    SRC_FILE="$SRC_GOAL/$L/pacp_L${L}.txt"
    DST_DIR="$DST_GOAL/$L"
    DST_FILE="$DST_DIR/pacp_L${L}.txt"

    echo -ne "\r進度: [ $COUNTER / $TOTAL_L ] 正在篩選 L=$L ...           "

    if [ -f "$SRC_FILE" ]; then
        # 核心修正：使用流式寫入避開變數長度限制
        awk -F',' -v t="$TARGET_PSL" '$2 == t' "$SRC_FILE" > "$TEMP_FILE"
        
        if [ -s "$TEMP_FILE" ]; then
            mkdir -p "$DST_DIR"
            mv "$TEMP_FILE" "$DST_FILE"
            M_COUNT=$(wc -l < "$DST_FILE")
            ((MATCH_TOTAL++))
            # 記錄成功的長度
            SUCCESS_L_LIST="${SUCCESS_L_LIST}${L},"
        else
            rm -f "$TEMP_FILE"
            M_COUNT=0
        fi
    else
        M_COUNT="N/A"
    fi

    # 暫存詳細統計表內容
    printf "%-10s | %-15s\n" "$L" "$M_COUNT" >> "$TEMP_REPORT"
done

# 4. 整合並輸出最終報表
{
    echo "PACP Optimal Selection Report (Goal1) - $(date)"
    echo "Selection Criteria: PSL == $TARGET_PSL"
    echo "=========================================="
    echo "[Success Summary]"
    if [ -z "$SUCCESS_L_LIST" ]; then
        echo "optimal_found_at: None"
    else
        echo "optimal_found_at: ${SUCCESS_L_LIST%,}"
    fi
    echo "Total Lengths with Solutions: $MATCH_TOTAL / $TOTAL_L"
    echo "=========================================="
    printf "%-10s | %-15s\n" "Length L" "Optimal Count"
    echo "------------------------------------------"
    cat "$TEMP_REPORT"
    echo "------------------------------------------"
    echo "End of Report"
} > "$REPORT"

# 清理暫存
rm -f "$TEMP_REPORT"

echo -e "\n\n\033[1;32m✅ 篩選完成！\033[0m"
echo -e "📄 報表位置: \033[1;36m$REPORT\033[0m"
echo -e "💡 你可以直接在報表頂部查看 \"optimal_found_at\" 清單。"