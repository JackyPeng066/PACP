#include "../lib/pacp_core.h"
#include <iostream>
#include <vector>
#include <set>
#include <algorithm>
#include <iomanip>

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./brute_force <OutFile> <L>" << std::endl;
        return 1;
    }

    std::string out_file = argv[1];
    int L = std::stoi(argv[2]);

    if (L > 18) { // L=18 已經非常極限，建議 L<=14
        std::cerr << "[Warning] L=" << L << " might be too slow for Brute Force." << std::endl;
    }

    // 2^L
    long long limit = 1LL << L;
    int min_psl = 999999; 
    
    std::vector<std::pair<Seq, Seq>> best_solutions;
    std::set<std::pair<std::string, std::string>> seen_canonical;

    Seq A(L), B(L);
    std::vector<int> acf_A(L), acf_B(L);

    long long total_ops = limit * limit / 2; // 因為 A 減半
    long long current_op = 0;
    
    std::cout << "--------------------------------------------------\n";
    std::cout << " BRUTE FORCE (OPTIMIZED) | L=" << L << "\n";
    std::cout << "--------------------------------------------------\n";

    // 外層迴圈: 遍歷 A
    for (long long i = 0; i < limit; ++i) {
        
        // [優化 1] 對稱性剪枝: 固定 A 的第一個元素為 +1 (即 bit 0 為 0)
        // 若 (i & 1) != 0，表示 A[0] 是 -1，跳過
        // int_to_seq 的實作是: bit 0 -> s[0]。若 bit=1 則 s=-1。
        if ((i & 1) != 0) continue; 

        int_to_seq(i, L, A);
        compute_acf(A, acf_A);

        // 內層迴圈: 遍歷 B
        for (long long j = 0; j < limit; ++j) {
            current_op++;
            
            // 進度條 (每 1M 次更新一次，避免 I/O 拖慢)
            if ((current_op & 0xFFFFF) == 0) {
                 std::cout << "\r[Scanning] " << std::fixed << std::setprecision(1) 
                           << (double)current_op/total_ops*100.0 << "% | Best PSL: " << min_psl << "   " << std::flush;
            }

            int_to_seq(j, L, B);
            
            // [優化 2] 提早離開 (Early Exit)
            // 我們不一次算完 ACF_B，而是邊算邊檢查 PSL
            int current_max_sidelobe = 0;
            bool possible_candidate = true;

            // 計算 acf_B 並檢查
            // 注意: 這裡手動展開 compute_acf + calc_psl 以達成提早離開
            for (int u = 1; u < L; ++u) {
                int sum_b = 0;
                for (int k = 0; k < L; ++k) {
                    sum_b += B[k] * B[(k + u) % L];
                }
                
                // 立即檢查 PSL
                int val = std::abs(acf_A[u] + sum_b);
                if (val > min_psl) {
                    possible_candidate = false;
                    break; // [關鍵] 只要有一個 u 爆掉，後面都不用算了
                }
                if (val > current_max_sidelobe) {
                    current_max_sidelobe = val;
                }
            }

            if (!possible_candidate) continue;

            // 如果活下來了，表示 current_max_sidelobe <= min_psl
            int psl = current_max_sidelobe;

            // 處理更佳解
            if (psl < min_psl) {
                min_psl = psl;
                best_solutions.clear();
                seen_canonical.clear();
                std::cout << "\r[Update] New Best PSL: " << min_psl << " found.            " << std::flush;
            }

            // 處理同級解 (唯一化)
            if (psl == min_psl) {
                std::string ca = get_canonical_repr(A);
                std::string cb = get_canonical_repr(B);
                if (ca > cb) std::swap(ca, cb);

                if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                    seen_canonical.insert({ca, cb});
                    best_solutions.push_back({A, B});
                }
            }
        }
    }

    std::cout << "\n--------------------------------------------------\n";
    std::cout << " DONE.\n";
    std::cout << " Best PSL Found: " << min_psl << "\n";
    std::cout << " Unique Pairs:   " << best_solutions.size() << "\n";
    std::cout << "--------------------------------------------------\n";

    save_result_list(out_file, best_solutions, L, min_psl);

    return 0;
}