#!/bin/bash
# 檔名: run_all_process.sh
# =========================================================
# PACP 全自動整合、篩選與報表系統 (完整版)
# =========================================================

BLUE='\033[1;34m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
NC='\033[0m'

START_TOTAL=$(date +%s)

echo -e "${BLUE}==============================================${NC}"
echo -e "${BLUE}   🚀 PACP 自動化處理系統 (含報表) 🚀   ${NC}"
echo -e "${BLUE}==============================================${NC}"

run_script() {
    local script_name=$1
    local description=$2
    echo -e "\n${YELLOW}[$(date +%H:%M:%S)] 執行: $description ...${NC}"
    if [ -f "$script_name" ]; then
        chmod +x "$script_name"
        ./"$script_name"
    else
        echo -e "${RED}❌ 錯誤：找不到 $script_name${NC}"; exit 1
    fi
}

# --- 階段一：資料整合 (依照 L 分流) ---
echo -e "\n${BLUE}--- 階段一：資料整合 (Merge) ---${NC}"
run_script "merge12.sh" "T1 系列整合 (By Length)"
run_script "merge3.sh"  "T2 系列整合 (By Length)"

# --- 階段二：篩選與驗證 (依照 L 驗證) ---
echo -e "\n${BLUE}--- 階段二：篩選與驗證 (Filter) ---${NC}"
run_script "optimal1.sh" "Goal 1 驗證 (Odd)"
run_script "optimal2.sh" "Goal 2 驗證 (Even)"
run_script "optimal3.sh" "Goal 3 篩選 (PQCP)"

# --- 階段三：生成報表 (新增) ---
echo -e "\n${BLUE}--- 階段三：統計報表 (Report) ---${NC}"
run_script "summary.sh"  "生成最終統計報表"

# --- 結束 ---
END_TOTAL=$(date +%s)
echo -e "\n${BLUE}==============================================${NC}"
echo -e "${GREEN}🎉 全部完成！總耗時 $((END_TOTAL - START_TOTAL)) 秒${NC}"
echo -e "${BLUE}報表檔案：./final_report.txt${NC}"
echo -e "${BLUE}==============================================${NC}"

# 直接顯示報表內容給你看
if [ -f "final_report.txt" ]; then
    cat final_report.txt
fi