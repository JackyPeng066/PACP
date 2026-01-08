#!/bin/bash
# 檔名: merge3.sh
# 功能: T2 系列 (Goal 3 PQCP) 依照 L 分開整合
# 輸出: ./results-g/L<L>_T2_merged.txt

SOURCE_DIR="./results-c/results by computer"
OUTPUT_DIR="./results-g"
mkdir -p "$OUTPUT_DIR"

echo "啟動 T2 系列分流整合 (By Length)..."

# 1. 找出所有 PQCP 檔，提取 L
# 路徑範例: .../46_PACP/46_PQCP.txt -> 提取 46
find "$SOURCE_DIR" -path "*N96141066-T2-*/*_PACP/*_PQCP.txt" | \
    sed -E 's/.*\/([0-9]+)_PQCP\.txt/\1/' | sort -n | uniq | while read L; do

    OUT_FILE="${OUTPUT_DIR}/L${L}_T2_merged.txt"
    
    echo -ne "正在處理 L=${L} (PQCP) ... "
    
    # 2. 針對這個 L 合併
    find "$SOURCE_DIR" -type f -path "*N96141066-T2-*/${L}_PACP/${L}_PQCP.txt" -print0 | \
        xargs -0 cat | \
        sort | uniq > "$OUT_FILE"
        
    COUNT=$(wc -l < "$OUT_FILE")
    echo "完成 (筆數: $COUNT)"
done

echo "✅ T2 整合完畢。"