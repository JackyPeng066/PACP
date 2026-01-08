#!/bin/bash
# 檔名: merge12.sh
# 功能: T1 系列 (Goal 1 & 2) 依照 L 分開整合
# 輸出: ./results-g/L<L>_T1_merged.txt

SOURCE_DIR="./results-c/results by computer"
OUTPUT_DIR="./results-g"
mkdir -p "$OUTPUT_DIR"

echo "啟動 T1 系列分流整合 (By Length)..."

# 1. 找出所有 pacp_L*.txt，提取 L 的數字，並排序去重
# 路徑範例: .../results/27/pacp_L27.txt -> 提取 27
find "$SOURCE_DIR" -path "*N96141066-T1-*/results/*/pacp_L*.txt" | \
    grep -o "pacp_L[0-9]*" | grep -o "[0-9]*" | sort -n | uniq | while read L; do

    # 定義輸出檔名 (依照 L 分開)
    OUT_FILE="${OUTPUT_DIR}/L${L}_T1_merged.txt"
    
    echo -ne "正在處理 L=${L} ... "
    
    # 2. 針對這個 L，找出所有來源檔案並合併
    find "$SOURCE_DIR" -type f -path "*N96141066-T1-*/results/${L}/pacp_L${L}.txt" -print0 | \
        xargs -0 cat | \
        sort | uniq > "$OUT_FILE"
        
    COUNT=$(wc -l < "$OUT_FILE")
    echo "完成 (筆數: $COUNT)"
done

echo "✅ T1 整合完畢。"