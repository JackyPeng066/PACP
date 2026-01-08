#!/bin/bash
# 檔名: optimal3.sh
# 功能: Goal 3 (PQCP) 篩選 - 輸出至 results-final/Goal3_PQCP/

INPUT_DIR="./results-g"
OUTPUT_DIR="./results-final/Goal3_PQCP"
mkdir -p "$OUTPUT_DIR"

echo "啟動 Goal 3 (PQCP) 篩選 -> 輸出目錄: $OUTPUT_DIR"

# 掃描 T2 整合檔
find "$INPUT_DIR" -name "L*_T2_merged.txt" | sort -V | while read FILE; do
    L=$(basename "$FILE" | grep -o "[0-9]*" | head -1)
    OUT_FILE="${OUTPUT_DIR}/L${L}_Goal3.txt"
    
    # 篩選 Max=4 且 Peaks=2
    grep "Max=4" "$FILE" | grep "Peaks=2" > "$OUT_FILE"
    
    # 檢查是否有內容，無則刪除空檔
    if [ -s "$OUT_FILE" ]; then
        COUNT=$(wc -l < "$OUT_FILE")
        echo "   [Goal 3] L=$L: 存入 $COUNT 筆"
    else
        rm -f "$OUT_FILE"
    fi
done