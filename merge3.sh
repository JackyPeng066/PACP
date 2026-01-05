#!/bin/bash

# =========================================================
# merge3 v2.0 - PQCP 正式完整整合版 (CSV 轉 TXT)
# 來源: results-c/results by computer/N96141066-T2-x/results/L
# 目標: results-g/Goal3/ (轉換為 TXT)
# 報表: results-g/PQCP_summary_report.txt
# =========================================================

# 1. 路徑設置
SRC_ROOT="results-c/results by computer"
DST_ROOT="results-g"
FOLDERS=("N96141066-T2-1" "N96141066-T2-2" "N96141066-T2-3")
GOAL3="$DST_ROOT/Goal3"
REPORT="$DST_ROOT/PQCP_summary_report.txt"

echo -e "\033[1;34m>>> 啟動 Goal3 (PQCP) 全量整合程序... <<<\033[0m"

# 建立目標目錄
mkdir -p "$GOAL3"

# 2. 獲取所有 T2 系列的 L 清單 (含 44-PQCP 等名稱)
echo "正在掃描 T2 來源目錄..."
ALL_L=$(for F in "${FOLDERS[@]}"; do 
    find "$SRC_ROOT/$F/results" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | xargs -n 1 basename
done | grep -E "^[0-9]+" | sort -u)

TOTAL_L=$(echo "$ALL_L" | wc -w)
if [ "$TOTAL_L" -eq 0 ]; then
    echo -e "\033[1;31m❌ 錯誤：找不到任何 T2 來源資料！\033[0m"
    exit 1
fi
echo "掃描完成，共有 $TOTAL_L 個 PQCP 相關目錄需要處理。"

# 初始化變數
PQCP_LIST=""
COUNTER=0

# 3. 核心處理循環
for L in $ALL_L; do
    ((COUNTER++))
    PQCP_LIST="${PQCP_LIST}${L},"
    
    DEST_DIR="$GOAL3/$L"
    mkdir -p "$DEST_DIR"

    # 定位來源
    L_PATHS=()
    for F in "${FOLDERS[@]}"; do
        P="$SRC_ROOT/$F/results/$L"
        [ -d "$P" ] && L_PATHS+=("$P")
    done

    # 取得 CSV 檔案清單並整合轉換
    ALL_FILES=$(for P in "${L_PATHS[@]}"; do ls -1 "$P" 2>/dev/null; done | grep ".csv$" | sort -u)
    
    for FILE in $ALL_FILES; do
        FILE_SOURCES=()
        for P in "${L_PATHS[@]}"; do [ -f "$P/$FILE" ] && FILE_SOURCES+=("$P/$FILE"); done
        
        # 轉換檔名格式
        NEW_FILENAME="${FILE%.csv}.txt"
        OUT_PATH="$DEST_DIR/$NEW_FILENAME"

        # 合併 -> 剔除標題 -> 排序 -> 去重 -> 輸出 TXT
        cat "${FILE_SOURCES[@]}" | grep -vE "L,PSL|L,max_s|psl|PSL" | sort | uniq > "$OUT_PATH"
    done

    # 低負載進度顯示
    echo -ne "\r處理進度: [ $COUNTER / $TOTAL_L ] 正在轉換 L=$L ...         "
done

echo -e "\n\n\033[1;32m整合與轉換完成，正在生成 PQCP 報表...\033[0m"

# 4. 生成 PQCP_summary_report.txt
{
    echo "PACP/PQCP Merge Summary Report (Goal3) - $(date)"
    echo "=========================================="
    echo "[PQCP List Summary]"
    echo "pqcp_lengths: ${PQCP_LIST%,}"
    echo "=========================================="
    printf "%-25s | %-15s\n" "PQCP Folder Name" "Total Results"
    echo "------------------------------------------"
    
    for L in $ALL_L; do
        D_DIR="$GOAL3/$L"
        # 統計該目錄下所有 txt 的總行數 (通常是一個主要的 txt)
        # 如果有多個 txt，會統計所有 txt 的非空行總數
        COUNT=$(cat "$D_DIR"/*.txt 2>/dev/null | grep -c "[^[:space:]]")
        [ -z "$COUNT" ] && COUNT="0"
        
        printf "%-25s | %-15s\n" "$L" "$COUNT"
    done
} > "$REPORT"

echo -e "\033[1;32m✅ Goal3 全量整合作業成功！\033[0m"
echo -e "📂 整合結果: $GOAL3"
echo -e "📄 專屬報表: $REPORT"