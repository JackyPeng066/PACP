#!/bin/bash

# =========================================================
# run_all_process.sh - PACP 數據整合與篩選總管
# 執行順序：整合 (T1/T2) -> 篩選 (Goal1/2/3)
# =========================================================

# 設定顏色輸出
BLUE='\033[1;34m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m' # 無顏色

# 記錄開始總時間
START_TOTAL=$(date +%s)

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   🚀 啟動 PACP 全自動整合與篩選流程 🚀   ${NC}"
echo -e "${BLUE}==============================================${NC}"

# 執行函式：檢查腳本是否存在並執行
run_script() {
    local script_name=$1
    local description=$2
    
    echo -e "\n${YELLOW}[階段 $(date +%H:%M:%S)] 正在執行: $description ($script_name)...${NC}"
    
    if [ -f "$script_name" ]; then
        chmod +x "$script_name"
        ./"$script_name"
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✅ $description 完成！${NC}"
        else
            echo -e "${RED}❌ $description 執行過程中出錯，請檢查邏輯。${NC}"
            exit 1
        fi
    else
        echo -e "${RED}❌ 錯誤：找不到腳本 $script_name，請確認檔案在同一目錄。${NC}"
        exit 1
    fi
}

# --- 第一階段：原始資料整合 (results-c -> results-g) ---
run_script "merge12.sh" "T1 系列整合 (Goal1 & Goal2)"
run_script "merge3.sh"  "T2 系列整合 (Goal3 PQCP)"

# --- 第二階段：最優解篩選 (results-g -> results-final) ---
run_script "optimal1.sh" "Goal1 最優解篩選 (PSL=2)"
run_script "optimal2.sh" "Goal2 最優解篩選 (PSL=4)"
run_script "optimal3.sh" "Goal3 PQCP 碎片整合與篩選"

# 記錄結束總時間
END_TOTAL=$(date +%s)
DURATION=$((END_TOTAL - START_TOTAL))

echo -e "\n${BLUE}==============================================${NC}"
echo -e "${GREEN}🎉 所有程序執行完畢！${NC}"
echo -e "${BLUE}總耗時：${DURATION} 秒${NC}"
echo -e "${BLUE}結果目錄：./results-final/${NC}"
echo -e "${BLUE}==============================================${NC}"

# 顯示最終報表清單
echo -e "${YELLOW}最終報表清單：${NC}"
ls -lh results-final/*_summary.txt 2>/dev/null