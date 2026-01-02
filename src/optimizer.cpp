#include "../lib/pacp_core.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <set>
#include <algorithm>

// 此處需保留 pacp_core.h 定義的 delta function
// 若 lib/pacp_core.cpp 沒公開 get_acf_diff, 這裡需要補上或 include 完整
// 假設您已在 pacp_core.h 加上 get_acf_diff 的宣告 (同之前)

void get_acf_diff(const Seq& s, int p, int L, std::vector<int>& diff_out) {
    int s_p = s[p];
    for (int u = 1; u < L; ++u) {
        int idx_plus = (p + u) % L;
        int idx_minus = (p - u + L) % L;
        diff_out[u] = -2 * s_p * (s[idx_plus] + s[idx_minus]);
    }
}

// ... Solver 類別與之前類似，但新增 PSL 追蹤 ...
// 為了簡潔，這裡重點展示主迴圈的修改

int main(int argc, char* argv[]) {
    if (argc < 4) return 1; 
    // Usage: ./optimizer <Seed> <Out> <FixA> <Count> <TargetVal>

    std::string in_file = argv[1];
    std::string out_file = argv[2];
    bool fix_a = (std::stoi(argv[3]) == 1);
    int target_count = std::stoi(argv[4]);
    int target_val = std::stoi(argv[5]);

    Seq A, B;
    if (!load_result(in_file, A, B)) return 1;
    int L = A.size();

    // 強制設定
    if(L < 20) fix_a = false; 

    // 唯一化儲存區
    std::set<std::pair<std::string, std::string>> seen_canonical;
    std::vector<std::pair<Seq, Seq>> results_buffer;

    // SA 變數
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist_idx(0, L - 1);
    std::uniform_real_distribution<double> dist_01(0.0, 1.0);
    std::vector<int> acf_A(L), acf_B(L), diff_buf(L);

    // 初始化狀態
    compute_acf(A, acf_A); compute_acf(B, acf_B);
    long long current_cost = calc_mse_cost(acf_A, acf_B, L, target_val);
    int current_psl = calc_psl(acf_A, acf_B, L);

    int best_known_psl = 9999;
    
    // 主迴圈
    int found_count = 0;
    long long steps = 0;
    double temp = 2.0;
    
    // 預設跑 1000 萬次迭代，或找到足夠數量
    long long MAX_STEPS = 10000000;
    
    while(steps < MAX_STEPS && (target_count == 0 || found_count < target_count)) {
        steps++;
        
        // 嘗試翻轉
        bool flip_a = fix_a ? false : (dist_01(rng) < 0.5);
        Seq& target_s = flip_a ? A : B;
        std::vector<int>& target_acf = flip_a ? acf_A : acf_B;
        std::vector<int>& other_acf = flip_a ? acf_B : acf_A;
        
        int idx = dist_idx(rng);
        get_acf_diff(target_s, idx, L, diff_buf);

        // 計算 Cost 變化 (Delta)
        long long new_cost = 0;
        // 這裡需要快速計算 MSE，不重算 PSL (PSL太慢)
        // 僅當 Cost 低時才檢查 PSL
        for(int u=1; u<L; ++u) {
            int val = std::abs(target_acf[u] + other_acf[u] + diff_buf[u]);
            new_cost += (long long)((val - target_val)*(val - target_val));
        }

        bool accept = false;
        if(new_cost <= current_cost) accept = true;
        else if(dist_01(rng) < std::exp((double)(current_cost - new_cost)/temp)) accept = true;

        if(accept) {
            target_s[idx] *= -1;
            for(int u=1; u<L; ++u) target_acf[u] += diff_buf[u];
            current_cost = new_cost;
            
            // 檢查是否為優良解
            // 條件：Cost 極低 (接近0)，或者 Cost 比之前看過的都好
            if (current_cost <= 2) { // 寬鬆一點，允許次佳
                 int psl = calc_psl(acf_A, acf_B, L);
                 
                 if (psl <= best_known_psl) {
                     if (psl < best_known_psl) {
                         // 發現更強的 PSL，清空舊的
                         best_known_psl = psl;
                         seen_canonical.clear();
                         results_buffer.clear();
                         found_count = 0;
                         std::cout << "\r[New Best PSL] " << psl << "  " << std::flush;
                     }
                     
                     // 唯一化檢查
                     std::string ca = get_canonical_repr(A);
                     std::string cb = get_canonical_repr(B);
                     if (ca > cb) std::swap(ca, cb);
                     
                     if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                         seen_canonical.insert({ca, cb});
                         results_buffer.push_back({A, B});
                         found_count++;
                         
                         // 暴力重置 (Hit and Run)
                         std::uniform_int_distribution<int> r(0,1);
                         for(int k=0; k<L; ++k) A[k] = r(rng)?1:-1;
                         for(int k=0; k<L; ++k) B[k] = r(rng)?1:-1;
                         compute_acf(A, acf_A); compute_acf(B, acf_B);
                         current_cost = calc_mse_cost(acf_A, acf_B, L, target_val);
                         temp = 5.0; // 重置溫度
                     }
                 }
            }
        }
        
        temp *= 0.999995;
        if(temp < 0.01) {
            temp = 2.0; 
            // 隨機踢一腳
            int k = dist_idx(rng); A[k]*=-1; 
            compute_acf(A, acf_A); current_cost = calc_mse_cost(acf_A, acf_B, L, target_val);
        }
    }

    // 最後一次性寫入
    save_result_list(out_file, results_buffer, L, best_known_psl);
    std::cout << "\n[Done] Saved " << results_buffer.size() << " unique pairs. Best PSL=" << best_known_psl << std::endl;
    return 0;
}