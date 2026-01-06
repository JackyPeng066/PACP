// ==========================================
// Filename: pqcp_tuner.h
// Optimization: Exact Number-Theoretic Targeting
// ==========================================

#ifndef PQCP_TUNER_H
#define PQCP_TUNER_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <random>

// --- [Part 1: 動態參數配置] ---
struct SolverConfig {
    int L;
    int stagnation_limit;    // 基礎爬山耐心
    int plateau_limit;       // 高原遊走耐心 (Goal 3 關鍵)
    int min_kick;            // 最小踢擊
    int max_kick;            // 最大踢擊
    int block_size;          // 區塊突變大小

    SolverConfig(int length) : L(length) {
        // [策略核心] 
        // 1. 基礎耐心：針對 i7-14700 高速運算，給予適當空間
        stagnation_limit = L * 600;  
        
        // 2. 高原耐心：這是 Goal 3 最貴的參數
        // 因為是固定 15% 機率遊走，需要足夠的步數來覆蓋
        plateau_limit = L * 3000; 

        // 3. 踢擊力度：動態計算
        min_kick = 2;
        max_kick = std::max(4, L / 8); 

        // 4. 區塊突變：約 10% ~ 15% 長度，用於種子修復與高原跳脫
        block_size = std::max(4, L / 8);
    }
};

// --- [Part 2: 數論種子修復器] ---
namespace SeedRefiner {
    
    inline int get_sum(const std::vector<int8_t>& seq) {
        int s = 0;
        for(auto x : seq) s += x;
        return s;
    }

    // [優化] 智慧型收斂：只翻轉那些能讓 Sum 靠近 Target 的位置
    template <typename RNG>
    void force_sum_convergence(std::vector<int8_t>& seq, int target_sum, RNG& rng, int L) {
        int current_sum = get_sum(seq);
        int max_attempts = 5000;

        while (current_sum != target_sum && max_attempts-- > 0) {
            // 判斷我們需要增加還是減少 Sum
            int direction = (target_sum > current_sum) ? 1 : -1;
            // direction = 1: 需要把 -1 變成 +1 (Sum+2)
            // direction = -1: 需要把 +1 變成 -1 (Sum-2)
            int target_val_to_flip = (direction == 1) ? -1 : 1;

            // [效率優化] 不再盲目隨機，我們嘗試直到找到一個正確的值
            int idx = rng.next_int(L);
            int attempts = 0;
            // 快速尋找可翻轉點
            while (seq[idx] != target_val_to_flip && attempts < L) {
                idx = (idx + 1) % L;
                attempts++;
            }

            if (seq[idx] == target_val_to_flip) {
                seq[idx] = -seq[idx]; // 翻轉
                current_sum += (direction * 2); // 直接更新 Sum
            }
        }
    }

    // [優化] 尋找離當前種子最近的合法能量組合
    template <typename RNG>
    void repair_seed_for_pqcp(std::vector<int8_t>& A, std::vector<int8_t>& B, int L, RNG& rng) {
        int SA = get_sum(A);
        int SB = get_sum(B);
        
        long long base = 2 * L;
        // PQCP 合法能量目標：2L, 2L+8, 2L-8
        std::vector<long long> valid_energies = {base, base + 8, base - 8};
        
        // 1. 建立所有可能的合法整數解 (Valid Targets)
        struct TargetPair { int ta; int tb; int dist; };
        std::vector<TargetPair> candidates;

        // 掃描範圍：Imbalance 通常不會超過 2*sqrt(L)
        int scan_limit = (int)std::sqrt(2.0 * L) + 8;
        
        for (int ta = -scan_limit; ta <= scan_limit; ++ta) {
            // 奇偶性檢查：Sum 必須與 L 同奇偶
            if (std::abs(ta % 2) != (L % 2)) continue;

            for (int tb = -scan_limit; tb <= scan_limit; ++tb) {
                if (std::abs(tb % 2) != (L % 2)) continue;

                long long e = (long long)ta * ta + (long long)tb * tb;
                
                // 檢查是否符合 PQCP 能量特徵
                bool match = false;
                for (auto ve : valid_energies) if (e == ve) match = true;

                if (match) {
                    // 計算曼哈頓距離 (Manhattan Distance)，找出改動最小的目標
                    int dist = std::abs(ta - SA) + std::abs(tb - SB);
                    candidates.push_back({ta, tb, dist});
                }
            }
        }

        if (candidates.empty()) return; // 極端情況 (L太小)，不做修復

        // 2. 選擇距離最近的目標 (Minimize Mutation)
        std::sort(candidates.begin(), candidates.end(), [](const TargetPair& a, const TargetPair& b) {
            return a.dist < b.dist;
        });

        // 為了保留一點隨機性，不一定選最近的，選前 3 近的隨機一個
        int pick = 0;
        if (candidates.size() > 1) {
            pick = rng.next_int(std::min((int)candidates.size(), 3));
        }
        
        int best_SA = candidates[pick].ta;
        int best_SB = candidates[pick].tb;

        // 3. 執行精確修復
        force_sum_convergence(A, best_SA, rng, L);
        force_sum_convergence(B, best_SB, rng, L);
    }
}

#endif