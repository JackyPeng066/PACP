#!/bin/bash

[ "$#" -ne 1 ] && { echo "Usage: $0 <L>"; exit 1; }

L=$1
START_TIME=$(date +%s)
# 採用動態 MAX_RHO = ceil(L/3)
MAX_RHO=$(( (L + 2) / 3 ))

echo ">>> OPTIMIZED PACP SEARCH | L=$L | REPRESENTATIVE MODE"

# 1. 取得權重但只取第一組作為代表
./bin/aps_weight $L || exit 1
W_FILE="results/$L/${L}_weight.txt"
READ_PAIR=$(head -n 1 "$W_FILE" | tr -d '\r')
read -r g0 g1 <<< "$READ_PAIR"

if [ -z "$g0" ]; then echo "Error: No weights found."; exit 1; fi

echo ">>> SELECTED REPRESENTATIVE WEIGHT PAIR: ($g0, $g1)"
DIR="results/$L/${g0}_${g1}"

# 2. DFS (只跑這一組)
./bin/aps_dfs $L $g0 $g1 $MAX_RHO

# 3. MATCH
if [ -f "$DIR/cand_g$g0.txt" ] && [ -f "$DIR/cand_g$g1.txt" ]; then
    ./bin/aps_match $L $g0 $g1
    
    # 4. FILTER
    if [ -s "$DIR/match_result.txt" ]; then
        ./bin/aps_filter $L $g0 $g1
    fi
else
    echo "Search failed to find candidates."
fi

# 5. SUMMARY
SUM_FILE="results/$L/summary.txt"
{
    echo "PACP SINGLE-PAIR REPORT | L=$L"
    echo "-------------------------------------------------------"
    printf "%-15s | %-12s | %-12s\n" "Weight($g0,$g1)" "RawPairs" "UniquePairs"
    echo "-------------------------------------------------------"
    D="results/$L/${g0}_${g1}"
    RAW=$(grep -c "[+-]" "$D/match_result.txt" 2>/dev/null || echo 0)
    UNIQ=$(grep -c "[+-]" "$D/unique_result.txt" 2>/dev/null || echo 0)
    printf "%-15s | %-12s | %-12s\n" "($g0,$g1)" "$RAW" "$UNIQ"
    echo "-------------------------------------------------------"
    echo "Total Time: $(($(date +%s) - START_TIME))s"
} > "$SUM_FILE"

cat "$SUM_FILE"