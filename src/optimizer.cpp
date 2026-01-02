#include "../lib/pacp_core.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <set>
#include <algorithm>
#include <cmath>

// 輔助函式: 快速計算 Cost (MSE)
long long fast_cost(const Seq& A, const Seq& B, int L, int target_val, std::vector<int>& buf_a, std::vector<int>& buf_b) {
    compute_acf(A, buf_a);
    compute_acf(B, buf_b);
    return calc_mse_cost(buf_a, buf_b, L, target_val);
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1; 

    std::string in_file = argv[1];
    std::string out_file = argv[2];
    bool fix_a = (std::stoi(argv[3]) == 1);
    int target_count = std::stoi(argv[4]);
    int target_val = std::stoi(argv[5]);

    Seq A, B;
    if (!load_result(in_file, A, B)) return 1;
    int L = A.size();

    // 強制設定: 小長度不鎖 A (避免邏輯死鎖)
    if(L < 20) fix_a = false; 

    // [參數浮動策略 1] 動態降溫係數 Alpha
    // L 越大，降溫必須越慢，否則像是在煮夾生飯
    // 經驗公式: Alpha = 1 - (1 / (Scale * L))
    double alpha = 1.0 - (1.0 / (5000.0 * L));
    
    // [參數浮動策略 2] 最大擾動幅度 (Max Flip Count)
    // L=100 時允許一次翻 5 bit; L=29 時翻 1-2 bit
    int max_mutation_k = std::max(1, (int)(L * 0.05));

    // [參數浮動策略 3] 能量標準化因子
    // 用於 Boltzmann 分佈，避免 L 很大時 Cost 很大導致機率驟降
    double cost_norm_factor = 1.0 / (double)L;

    std::cout << "--------------------------------------------------\n";
    std::cout << " DYNAMIC OPTIMIZER | L=" << L << " | Target=" << target_val << "\n";
    std::cout << " Alpha=" << alpha << " | MaxFlip=" << max_mutation_k << " | NormFactor=" << cost_norm_factor << "\n";
    std::cout << " Strategy: " << (fix_a ? "Legendre Phase Floating (Rotate A, Flip B)" : "Co-evolution (Flip A & B)") << "\n";
    std::cout << "--------------------------------------------------\n";

    // 隨機數
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist_01(0.0, 1.0);
    std::uniform_int_distribution<int> dist_idx(0, L - 1);
    std::uniform_int_distribution<int> dist_rot(1, L - 1);

    // 緩衝區
    std::vector<int> acf_A(L), acf_B(L);
    long long current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
    int best_psl = 99999;

    // 儲存結果
    std::vector<std::pair<Seq, Seq>> results_buffer;
    std::set<std::pair<std::string, std::string>> seen_canonical;
    int found_count = 0;

    // SA 參數
    double temp = 5.0; // 初始高溫
    long long step = 0;
    long long max_steps = 100000000; // 給予足夠多的步數，依靠 timeout 腳本來終止
    
    // 重啟計數器
    int stuck_counter = 0;
    int stuck_threshold = 2000 * L; // L 越大，容忍越久的平原期

    while(step < max_steps && (target_count == 0 || found_count < target_count)) {
        step++;
        
        // 備份狀態 (用於 Revert)
        Seq old_A = A;
        Seq old_B = B;
        long long old_cost = current_cost;

        // --- 動作選擇 ---
        // 95% 機率是翻轉 (Mutation)，5% 機率是旋轉 (Rotation - Phase Floating)
        bool is_rotate_move = (dist_01(rng) < 0.05);

        if (is_rotate_move) {
            // [浮動策略 3] 相位浮動
            // 即使 Fix A，我們也允許 A 旋轉 (Legendre 的不同相位)
            // 如果不 Fix A，則隨機選 A 或 B 旋轉
            int rot_k = dist_rot(rng);
            if (fix_a) {
                rotate_seq_left(A, rot_k);
            } else {
                if (dist_01(rng) < 0.5) rotate_seq_left(A, rot_k);
                else rotate_seq_left(B, rot_k);
            }
        } else {
            // [浮動策略 2] 動態擾動
            // 溫度越高，翻轉的 bit 數越多 (粗調 -> 精調)
            // k 在 [1, max_mutation_k] 之間浮動
            int current_k = 1;
            if (temp > 1.0 && max_mutation_k > 1) {
                std::uniform_int_distribution<int> dist_k(1, max_mutation_k);
                current_k = dist_k(rng);
            }

            // 執行 k 次翻轉
            for(int k=0; k<current_k; ++k) {
                // 決定翻誰
                bool flip_a = fix_a ? false : (dist_01(rng) < 0.5);
                if (flip_a) A[dist_idx(rng)] *= -1;
                else        B[dist_idx(rng)] *= -1;
            }
        }

        // 計算新 Cost (這裡為了多點翻轉正確性，使用全重算，L<100 速度夠快)
        long long new_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);

        // --- SA 接受準則 ---
        bool accept = false;
        if (new_cost <= current_cost) {
            accept = true;
        } else {
            // [浮動策略 3] 使用標準化 Cost 計算機率
            double delta = (double)(new_cost - current_cost) * cost_norm_factor;
            if (dist_01(rng) < std::exp(-delta / temp)) {
                accept = true;
            }
        }

        if (accept) {
            current_cost = new_cost;
            
            // 如果 Cost 有改善，重置卡住計數器
            if (new_cost < old_cost) stuck_counter = 0;
            else stuck_counter++;

            // 檢查最佳解
            // 這裡設定一個較寬鬆的閾值來觸發 PSL 檢查 (例如 MSE < L/2)
            if (current_cost <= (L/2)) {
                int psl = calc_psl(acf_A, acf_B, L);
                
                // 發現新紀錄
                if (psl < best_psl) {
                    best_psl = psl;
                    results_buffer.clear();
                    seen_canonical.clear();
                    found_count = 0; // 重置計數，因為我們想要的是"最佳"的 n 組
                    std::cout << "\r[New Best] Step " << step << " | PSL=" << best_psl << " (MSE=" << current_cost << ")    " << std::flush;
                }

                // 紀錄解
                if (psl == best_psl) {
                    std::string ca = get_canonical_repr(A);
                    std::string cb = get_canonical_repr(B);
                    if (ca > cb) std::swap(ca, cb);
                    
                    if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                        seen_canonical.insert({ca, cb});
                        results_buffer.push_back({A, B});
                        found_count++;

                        // 如果達到目標且 PSL 夠好 (奇數=2, 偶數=0)，提早結束
                        // 注意: 有些長度可能數學上達不到 2，所以我們只做存檔，不強制 break，除非是 0
                        if (target_val == 0 && best_psl == 0 && found_count >= target_count) break;
                        if (target_val == 2 && best_psl == 2 && found_count >= target_count) break;
                    }
                }
            }
        } else {
            // 拒絕: 還原狀態
            A = old_A;
            B = old_B;
            current_cost = old_cost;
            stuck_counter++;
        }

        // --- 降溫與重啟 ---
        temp *= alpha;

        // 防卡死重啟機制 (Restart)
        if (stuck_counter > stuck_threshold || temp < 0.001) {
            // 溫度重置
            temp = 5.0;
            stuck_counter = 0;
            
            // 結構重置 (保留當前已知最好的結構，但做大擾動)
            // 這裡我們選擇：完全隨機重置 B (若是 FixA)，或者重置 A,B (若非 FixA)
            // 這是為了跳出深層局部解
            if (fix_a) {
                // A 轉一轉，B 重洗
                rotate_seq_left(A, dist_rot(rng)); 
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
            } else {
                for(int i=0; i<L; ++i) A[i] = (dist_01(rng)<0.5)?1:-1;
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
            }
            // 重算 Cost
            current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
        }

        // 進度顯示
        if (step % 100000 == 0) {
            // 簡單跳動點點
            // std::cout << "." << std::flush;
        }
    }

    // 儲存結果
    std::cout << "\n[Done] Best PSL Found: " << best_psl << " | Unique Count: " << results_buffer.size() << std::endl;
    save_result_list(out_file, results_buffer, L, best_psl);

    return 0;
}