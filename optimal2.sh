#!/bin/bash
# 檔名: optimal2.sh
# 功能: Goal 2 (Even) 篩選 - 輸出至 results-final/Goal2_Even/

INPUT_DIR="./results-g"
OUTPUT_DIR="./results-final/Goal2_Even"
mkdir -p "$OUTPUT_DIR"

echo "啟動 Goal 2 (Even) 篩選 -> 輸出目錄: $OUTPUT_DIR"

find "$INPUT_DIR" -name "L*_T1_merged.txt" | sort -V | while read FILE; do
    L=$(basename "$FILE" | grep -o "[0-9]*" | head -1)
    
    # 只處理偶數
    if [ $((L % 2)) -eq 0 ]; then
        OUT_FILE="${OUTPUT_DIR}/L${L}_Goal2.txt"
        
        # 呼叫 Python 驗證 (Optimal Even)
        python3 -c "
import sys
L = int('$L')
try:
    with open('$FILE', 'r') as fin, open('$OUT_FILE', 'w') as fout:
        count = 0
        half_L = L // 2
        for line in fin:
            line = line.strip()
            parts = line.replace(',', ' ').split()
            if len(parts) < 4: continue
            
            seqA = [1 if c in '+1' else -1 for c in parts[2] if c in '+-10']
            seqB = [1 if c in '+1' else -1 for c in parts[3] if c in '+-10']
            
            # 驗證 Even Optimal (ZCZ=0, Peak@L/2=4)
            valid = True
            for u in range(1, L):
                val = 0
                for i in range(L):
                    val += seqA[i]*seqA[(i+u)%L] + seqB[i]*seqB[(i+u)%L]
                
                abs_val = abs(val)
                if u == half_L:
                    if abs_val != 4: 
                        valid = False
                        break
                else:
                    if abs_val != 0:
                        valid = False
                        break
            
            if valid:
                fout.write(line + '\n')
                count += 1
        if count > 0:
            print(f'   [Goal 2] L={L}: 存入 {count} 筆', file=sys.stderr)
except Exception:
    pass
"
    fi
done