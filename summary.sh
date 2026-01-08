#!/bin/bash
# 檔名: summary.sh
# 功能: 掃描 results-final 下的分類資料夾，生成統計報表

BASE_DIR="./results-final"
REPORT_FILE="final_report.txt"

# 建立報表標頭
echo "========================================================" > "$REPORT_FILE"
echo "               PACP 搜尋結果統計總表                    " >> "$REPORT_FILE"
echo "      時間: $(date '+%Y-%m-%d %H:%M:%S')              " >> "$REPORT_FILE"
echo "========================================================" >> "$REPORT_FILE"

# 定義掃描函數
scan_goal_folder() {
    local folder_name=$1
    local title=$2
    local target_path="$BASE_DIR/$folder_name"
    
    echo "" >> "$REPORT_FILE"
    echo "[$title]" >> "$REPORT_FILE"
    printf "%-8s | %-8s | %s\n" "Length" "Count" "Filename" >> "$REPORT_FILE"
    echo "---------|----------|---------------------------------" >> "$REPORT_FILE"
    
    if [ -d "$target_path" ]; then
        # 找出該資料夾下所有 txt 檔並排序
        local files=$(find "$target_path" -name "*.txt" | sort -V)
        
        if [ -z "$files" ]; then
            echo "  (無資料)" >> "$REPORT_FILE"
        else
            for FILE in $files; do
                # 提取 L
                L=$(basename "$FILE" | grep -o "L[0-9]*" | grep -o "[0-9]*")
                # 計算行數
                COUNT=$(wc -l < "$FILE")
                # 取得檔名
                FNAME=$(basename "$FILE")
                
                printf "%-8s | %-8s | %s\n" "$L" "$COUNT" "$FNAME" >> "$REPORT_FILE"
            done
        fi
    else
        echo "  (目錄未建立)" >> "$REPORT_FILE"
    fi
}

# 依序掃描三個目標
scan_goal_folder "Goal1_Odd"  "Goal 1 (Odd Optimal)"
scan_goal_folder "Goal2_Even" "Goal 2 (Even Optimal)"
scan_goal_folder "Goal3_PQCP" "Goal 3 (PQCP Specific)"

# 結尾統計
echo "" >> "$REPORT_FILE"
echo "========================================================" >> "$REPORT_FILE"
TOTAL_FILES=$(find "$BASE_DIR" -name "*.txt" | grep -v "report" | wc -l)
echo "總檔案數: $TOTAL_FILES" >> "$REPORT_FILE"

echo "報表生成完畢: $REPORT_FILE"