#!/bin/bash

# =========================================================
# optimal3 v3.2 - PQCP 全量整合版 (流式合併 + 格式統一)
# 來源: results-g/Goal3/ (各長度資料夾)
# 目標: results-final/Goal3/L??_PQCP.txt
# 報表: results-final/optimal3_summary.txt
# =========================================================

# 1. 設置路徑
SRC_GOAL="results-g/Goal3"
DST_ROOT="results-final"
DST_GOAL="$DST_ROOT/Goal3"
REPORT="$DST_ROOT/optimal3_summary.txt"

echo -e "\033[1;34m>>> 啟動 Goal3 (PQCP) 全量篩選整合程序 <<<\033[0m"

# 確保目標根目錄存在
mkdir -p "$DST_GOAL"

# 2. 獲取 L 資料夾清單 (例如 44_PQCP)
ALL_L_FOLDERS=$(ls -1 "$SRC_GOAL" 2>/dev/null | grep "_PQCP" | sort -V)
TOTAL_F=$(echo "$ALL_L_FOLDERS" | wc -w)

if [ "$TOTAL_F" -eq 0 ]; then
    echo -e "\033[1;31m❌ 錯誤：在 $SRC_GOAL 中找不到任何 PQCP 資料夾！\033[0m"
    exit 1
fi

# 初始化暫存變數
COUNTER=0
MATCH_TOTAL=0
SUCCESS_L_LIST=""
TEMP_REPORT=".report3_temp"

# 3. 執行循環處理
for FOLDER in $ALL_L_FOLDERS; do
    ((COUNTER++))
    
    # 提取純數字 L
    L_NUM=$(echo "$FOLDER" | grep -oE "^[0-9]+")
    
    SRC_DIR="$SRC_GOAL/$FOLDER"
    DST_FILE="$DST_GOAL/L${L_NUM}_PQCP.txt"

    echo -ne "\r進度: [ $COUNTER / $TOTAL_F ] 正在整合 L=$L_NUM ...           "

    # 4. 執行碎片檔案合併 (流式處理確保不丟失數據)
    # 使用 find 找出所有 txt，合併後排序並去重
    find "$SRC_DIR" -name "*.txt" -type f -exec cat {} + | sort | uniq > "$DST_FILE"

    # 檢查整合後是否有內容
    if [ -s "$DST_FILE" ]; then
        M_COUNT=$(wc -l < "$DST_FILE")
        ((MATCH_TOTAL++))
        # 只留下數字到清單中
        SUCCESS_L_LIST="${SUCCESS_L_LIST}${L_NUM},"
    else
        rm -f "$DST_FILE"
        M_COUNT=0
    fi

    # 寫入詳細統計到暫存表
    printf "%-15s | %-15s\n" "$L_NUM" "$M_COUNT" >> "$TEMP_REPORT"
done

# 5. 整合並輸出最終報表 (比照 Goal1/2 層級與格式)
{
    echo "PACP Optimal Selection Report (Goal3-PQCP) - $(date)"
    echo "Selection Criteria: Merged Fragmented Files"
    echo "=========================================="
    echo "[Success Summary]"
    if [ -z "$SUCCESS_L_LIST" ]; then
        echo "pqcp_found_at: None"
    else
        echo "pqcp_found_at: ${SUCCESS_L_LIST%,}"
    fi
    echo "Total Lengths with Solutions: $MATCH_TOTAL / $TOTAL_F"
    echo "=========================================="
    printf "%-15s | %-15s\n" "Length L" "Total Solutions"
    echo "------------------------------------------"
    [ -f "$TEMP_REPORT" ] && cat "$TEMP_REPORT"
    echo "------------------------------------------"
    echo "End of Report"
} > "$REPORT"

# 清理暫存檔案
rm -f "$TEMP_REPORT"

echo -e "\n\n\033[1;32m✅ Goal3 整合完成！\033[0m"
echo -e "📄 報表位置: \033[1;36m$REPORT\033[0m"
echo -e "📂 整合結果: \033[1;36m$DST_GOAL/ (例如 L44_PQCP.txt)\033[0m"