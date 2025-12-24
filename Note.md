pipline
1. hamming weight
    <g0,g1>
    each weight will get the same dfs
2. dfs
    psd (sampling)
3. match
4. filter


# Project 2 Pipeline & Data Flow Documentation
**Version:** Modular File-Based Architecture
**Date:** 2025-12-24

---

## 1. 系統架構概念 (System Concept)

本系統採用 **分離式架構 (Decoupled Architecture)**。各個 C++ 程式專注於單一任務，透過檔案系統傳遞資料。

* **Controller**: `driver.sh` (Shell Script) - 負責參數傳遞與流程控制。
* **Active Data Token**: `<L>` (目標長度)，貫穿所有模組。

---

## 2. 資料流與模組介面 (Data Flow & Interfaces)

### Phase 1: 權重分析 (Weight Analysis)
**目標**: 計算出所有可能的權重組合，作為後續搜尋的依據。

* **Executable**: `./aps_weight`
* **Input**: `<L>` (Integer)
* **Process**: 
    1.  檢查是否為 Golay Length (若有則跳過)。
    2.  計算 Adhikary 方程式：$(g_0 - g_1)^2 + (L - g_0 - g_1)^2 = L \pm 2$。
* **Output (File)**: `results/<L>/<L>_weight.txt`
    * *Format*: 每行 `<g0> <g1>` (例如: `11 12`)。

### Phase 2: 序列生成 (Sequence Generation)
**目標**: 針對特定權重產生所有候選序列 (Candidates)。

* **Executable**: `./aps_dfs` (需調整為 Output Mode)
* **Input**: `<L>`, `<g>`, `<Z>`, `<prefix>`, `<max_seq>`
* **Process**:
    1.  DFS 遞迴枚舉 $\pm 1$ 序列。
    2.  **Pruning**: Hamming Weight `<g>` & Prefix Canonical。
    3.  **PSD Calc**: 計算頻域能量 (Internal check)。
* **Output (File/Stream)**: `results/<L>/<g>_<g>/cand_g<g>.txt`
    * *Format*: 每行一條序列 `<seq>` (例如: `++-++--...`)。

### Phase 3: 配對搜尋 (Matching)
**目標**: 讀取兩組候選池，找出符合 PACP/PCP 條件的配對。

* **Executable**: `./aps_match`
* **Input**: `<L>`, `<g0>`, `<g1>`
* **Process**:
    1.  **Read**: 載入 `cand_g<g0>.txt` 與 `cand_g<g1>.txt`。
    2.  **Cross-Match**: 雙層迴圈 $O(N \times M)$。
    3.  **PACF Check**: 計算時域週期性自相關。
        * Odd L: $|Sum| \le 2$
        * Even L: $|Sum| \le 4$
* **Output (File)**: `results/<L>/<g0>_<g1>/match_result.txt`
    * *Format*: `<seqA>,<seqB>`。

### Phase 4: 唯一性過濾 (Uniqueness Filter)
**目標**: 去除重複的等價類 (Equivalence Classes)。

* **Executable**: `./aps_filter`
* **Input**: `<L>`, `<g0>`, `<g1>`
* **Process**:
    1.  **Read**: `match_result.txt`。
    2.  **Canonical Reduction**: 對每一對序列做 Shift / Reversal / Negation / Swap。
    3.  **Deduplicate**: 使用 `Set` 過濾重複。
* **Output (File)**: `results/<L>/<g0>_<g1>/unique_result.txt`
    * *Format*: `<seqA>,<seqB>` (最終結果)。

---

## 3. 資料流圖 (Data Flow Diagram)

```mermaid
graph TD
    User((User Input)) -->|"<L>"| Driver[./driver.sh]
    
    subgraph Stage 1: Weight
    Driver -->|"<L>"| BinWeight[./aps_weight]
    BinWeight -->|Write| FileWeight["results/<L>/<L>_weight.txt"]
    end
    
    subgraph Stage 2: Generation
    FileWeight -.->|Read <g0>, <g1>| Driver
    Driver -->|"<L> <g0>"| BinDFS[./aps_dfs]
    Driver -->|"<L> <g1>"| BinDFS
    BinDFS -->|Write| FileCand0["cand_g<g0>.txt"]
    BinDFS -->|Write| FileCand1["cand_g<g1>.txt"]
    end
    
    subgraph Stage 3: Match
    Driver -->|"<L> <g0> <g1>"| BinMatch[./aps_match]
    FileCand0 -.-> BinMatch
    FileCand1 -.-> BinMatch
    BinMatch -->|Write| FileMatch["match_result.txt"]
    end
    
    subgraph Stage 4: Filter
    Driver -->|"<L> <g0> <g1>"| BinFilter[./aps_filter]
    FileMatch -.-> BinFilter
    BinFilter -->|Write| FileUnique["unique_result.txt"]
    end

Project_Root/
├── Makefile                # 自動化編譯與執行腳本
├── src/
│   ├── main.cpp            # Stage 1: Weight Analysis Source
│   └── aps_dfs.cpp         # Stage 2: DFS Hunter Source
├── bin/                    # Compiled Executables
│   ├── weight_analysis.exe
│   └── aps_dfs.exe
└── results/                # Output Data
    ├── weight.txt          # [Critical] 由 Stage 1 生成，Stage 2 讀取
    └── <L>_out.txt         # (Example) 最終搜到的序列

# ./driver.sh <L>
L=$1

# 1. 計算權重
./aps_weight $L

# 2. 逐行讀取權重檔
while read g0 g1; do
    echo "Processing Pair: ($g0, $g1)"
    
    # 建立目錄
    DIR="results/$L/${g0}_${g1}"
    mkdir -p $DIR
    
    # 3. 生成序列 (若 g0 == g1 則只需跑一次)
    ./aps_dfs $L $g0 ... > "$DIR/cand_g${g0}.txt"
    if [ "$g0" != "$g1" ]; then
        ./aps_dfs $L $g1 ... > "$DIR/cand_g${g1}.txt"
    fi
    
    # 4. 執行配對
    ./aps_match $L $g0 $g1
    
    # 5. 過濾結果
    ./aps_filter $L $g0 $g1
    
done < "results/$L/${g_L}_weight.txt"