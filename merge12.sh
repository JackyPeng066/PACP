#!/bin/bash

# =========================================================
# merge12 v5.0 - 正式完整整合版 (高效、低負載、含報表)
# 來源: results-c/results by computer/N96141066-T1-x/results/L
# 目標: results-g/Goal1(2)/L
# =========================================================

# 1. 設置路徑
SRC_ROOT="results-c/results by computer"
DST_ROOT="results-g"
FOLDERS=("N96141066-T1-1" "N96141066-T1-2" "N96141066-T1-3")
REPORT="$DST_ROOT/summary_report.txt"
GOAL1="$DST_ROOT/Goal1"
GOAL2="$DST_ROOT/Goal2"

echo -e "\033[1;34m>>> 啟動全量整合程序... <<<\033[0m"

# 建立目錄結構
mkdir -p "$GOAL1" "$GOAL2"

# 2. 獲取所有數字 L 清單
echo "正在掃描來源目錄，請稍候..."
ALL_L=$(for F in "${FOLDERS[@]}"; do 
    find "$SRC_ROOT/$F/results" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | xargs -n 1 basename
done | grep -E "^[0-9]+$" | sort -u -n)

TOTAL_L=$(echo "$ALL_L" | wc -w)
echo "掃描完成，共有 $TOTAL_L 個長度需要處理。"

# 初始化報表變數
ODD_LIST=""
EVEN_LIST=""
COUNTER=0

# 3. 核心執行循環
for L in $ALL_L; do
    ((COUNTER++))
    
    # 分類判斷
    if (( L % 2 != 0 )); then
        DEST_DIR="$GOAL1/$L"
        ODD_LIST="${ODD_LIST}${L},"
    else
        DEST_DIR="$GOAL2/$L"
        EVEN_LIST="${EVEN_LIST}${L},"
    fi

    mkdir -p "$DEST_DIR"

    # 定位來源路徑
    L_PATHS=()
    for F in "${FOLDERS[@]}"; do
        P="$SRC_ROOT/$F/results/$L"
        [ -d "$P" ] && L_PATHS+=("$P")
    done

    # 取得檔案清單並整合
    ALL_FILES=$(for P in "${L_PATHS[@]}"; do ls -1 "$P" 2>/dev/null; done | sort -u)
    for FILE in $ALL_FILES; do
        FILE_SOURCES=()
        for P in "${L_PATHS[@]}"; do [ -f "$P/$FILE" ] && FILE_SOURCES+=("$P/$FILE"); done
        
        OUT_PATH="$DEST_DIR/$FILE"
        if [[ "$FILE" == *.txt ]]; then
            # 唯讀原始檔，排序去重寫入新檔
            cat "${FILE_SOURCES[@]}" | sort | uniq > "$OUT_PATH"
        else
            cat "${FILE_SOURCES[@]}" > "$OUT_PATH"
        fi
    done

    # 每隔 1 個 L 顯示一次單行進度，避免刷屏
    echo -ne "\r進度: [ $COUNTER / $TOTAL_L ] 正在處理 L=$L ...         "
done

echo -e "\n\n\033[1;32m整合運算完成，正在生成報表...\033[0m"

# 4. 生成 summary_report.txt
{
    echo "PACP Merge Summary Report - $(date)"
    echo "=========================================="
    echo "[L List Summary]"
    echo "odd: ${ODD_LIST%,}"
    echo "even: ${EVEN_LIST%,}"
    echo "=========================================="
    printf "%-10s | %-15s | %-10s\n" "Length L" "Category" "Results Count"
    echo "------------------------------------------"
    
    for L in $ALL_L; do
        if (( L % 2 != 0 )); then 
            CAT="Odd"; D_PATH="$GOAL1/$L"
        else 
            CAT="Even"; D_PATH="$GOAL2/$L"
        fi
        
        TARGET_DATA="$D_PATH/pacp_L${L}.txt"
        [ -f "$TARGET_DATA" ] && COUNT=$(grep -c "[^[:space:]]" "$TARGET_DATA") || COUNT="0"
        printf "%-10s | %-15s | %-10s\n" "$L" "$CAT" "$COUNT"
    done
} > "$REPORT"

echo -e "\033[1;32m✅ 全量整合作業成功！\033[0m"
echo -e "📂 整合結果目錄: $DST_ROOT"
echo -e "📄 數據報表檔案: $REPORT"