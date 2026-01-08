#!/bin/bash
# 檔名: optimal1.sh
# 功能: Goal 1 (Odd) 篩選 - 輸出至 results-final/Goal1_Odd/

INPUT_DIR="./results-g"
OUTPUT_DIR="./results-final/Goal1_Odd"
mkdir -p "$OUTPUT_DIR"

echo "啟動 Goal 1 (Odd) 篩選 -> 輸出目錄: $OUTPUT_DIR"

# 掃描 T1 整合檔
find "$INPUT_DIR" -name "L*_T1_merged.txt" | sort -V | while read FILE; do
    L=$(basename "$FILE" | grep -o "[0-9]*" | head -1)
    
    # 只處理奇數
    if [ $((L % 2)) -ne 0 ]; then
        OUT_FILE="${OUTPUT_DIR}/L${L}_Goal1.txt"
        
        # 呼叫 Python 驗證 (PSL=2)
        python3 -c "
import sys
L = int('$L')
try:
    with open('$FILE', 'r') as fin, open('$OUT_FILE', 'w') as fout:
        count = 0
        for line in fin:
            line = line.strip()
            parts = line.replace(',', ' ').split()
            if len(parts) < 4: continue
            
            # 解析序列
            seqA = [1 if c in '+1' else -1 for c in parts[2] if c in '+-10']
            seqB = [1 if c in '+1' else -1 for c in parts[3] if c in '+-10']
            
            # 驗證 Odd Optimal (所有旁瓣絕對值 == 2)
            valid = True
            for u in range(1, L):
                val = 0
                for i in range(L):
                    val += seqA[i]*seqA[(i+u)%L] + seqB[i]*seqB[(i+u)%L]
                if abs(val) != 2:
                    valid = False
                    break
            
            if valid:
                fout.write(line + '\n')
                count += 1
        if count > 0:
            print(f'   [Goal 1] L={L}: 存入 {count} 筆', file=sys.stderr)
except Exception:
    pass
"
    fi
done