/*
   PQCP Optimizer v26.0 - L44/46 Specialist
   Features:
     - Conjugate Pair Flip (Targeted correction for specific lags)
     - Dynamic Tabu Tensor
     - Max-Violation Guided Kick
   
   Warning: Highly aggressive optimization logic.
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <deque>
#include <cstring>
#include <tuple>
#include "../lib/pqcp_tuner.h" 

// --- 高效能 RNG ---
struct XorShift128 {
    uint32_t x, y, z, w;
    XorShift128(uint32_t seed) {
        x = seed; y = 362436069; z = 521288629; w = 88675123;
        for(int i=0; i<50; ++i) next(); 
    }
    inline uint32_t next() {
        uint32_t t = x ^ (x << 11);
        x = y; y = z; z = w;
        return w = w ^ (w >> 19) ^ t ^ (t >> 8);
    }
    inline int next_int(int range) { return next() % range; }
    inline double next_double() { return (double)next() / 4294967296.0; }
};

// --- 序列狀態 ---
class SequenceState {
public:
    int L;
    std::vector<int8_t> A, B; 
    std::vector<int8_t> ext_A, ext_B; // 3x Buffer
    std::vector<int> sum_rho;
    
    // Metrics
    int violations = 0; 
    long long total_energy = 0; 
    long long kurtosis_proxy = 0; 
    int max_violation_shift = 0; // [New] 紀錄最大違規的位置

    SequenceState(int length) : L(length) {
        A.resize(L); B.resize(L);
        ext_A.resize(3 * L);
        ext_B.resize(3 * L);
        sum_rho.resize(L);
    }

    void sync_buffers() {
        std::memcpy(ext_A.data(), A.data(), L);
        std::memcpy(ext_A.data() + L, A.data(), L);
        std::memcpy(ext_A.data() + 2*L, A.data(), L);

        std::memcpy(ext_B.data(), B.data(), L);
        std::memcpy(ext_B.data() + L, B.data(), L);
        std::memcpy(ext_B.data() + 2*L, B.data(), L);
    }

    void update_single_buffer(int seq_idx, int p, int8_t val) {
        int8_t* base = (seq_idx == 0) ? ext_A.data() : ext_B.data();
        base[p] = base[p+L] = base[p+2*L] = val;
    }

    void randomize(XorShift128& rng) {
        for(int i=0; i<L; ++i) {
            A[i] = (rng.next() & 1) ? 1 : -1;
            B[i] = (rng.next() & 1) ? 1 : -1;
        }
        sync_buffers();
    }

    void full_recalc() {
        std::fill(sum_rho.begin(), sum_rho.end(), 0);
        const int8_t* ptrA = ext_A.data() + L;
        const int8_t* ptrB = ext_B.data() + L;
        
        for (int u = 0; u < L; ++u) {
            int r = 0;
            #pragma GCC ivdep
            for (int i = 0; i < L; ++i) {
                r += ptrA[i] * ptrA[i + u] + ptrB[i] * ptrB[i + u];
            }
            sum_rho[u] = r;
        }
        update_metrics();
    }

    void update_metrics() {
        violations = 0;
        total_energy = 0;
        kurtosis_proxy = 0;
        max_violation_shift = 0;
        int max_val = 0;

        int limit = L / 2;
        for (int u = 1; u <= limit; ++u) {
            int abs_val = std::abs(sum_rho[u]);
            
            if (abs_val > 4) {
                violations++;
                // 追蹤最大違規位置，用於 Targeted Kick
                if (abs_val > max_val) {
                    max_val = abs_val;
                    max_violation_shift = u;
                }
            }
            total_energy += abs_val;
            if (abs_val > 0) kurtosis_proxy += (abs_val * abs_val); 
        }
    }

    bool is_strict_pqcp() const {
        if (violations > 0) return false;
        int peaks = 0;
        for (int u = 1; u < L; ++u) {
            int val = std::abs(sum_rho[u]);
            if (val != 0) {
                if (val != 4) return false;
                peaks++;
            }
        }
        return (peaks == 2);
    }

    struct EvalResult { int d_viol; int d_energy; int d_kurtosis; };

    // --- 單點翻轉評估 ---
    inline EvalResult evaluate_flip(int seq_idx, int p) const {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        
        int d_viol = 0; int d_energy = 0; int d_kurtosis = 0;
        int limit = L >> 1;
        const int* rho_ptr = sum_rho.data();

        for (int u = 1; u <= limit; ++u) {
            int sum_neighbors = center_ptr[u] + center_ptr[-u]; 
            if (sum_neighbors == 0) continue; 

            int delta = -2 * val_p * sum_neighbors;
            
            int abs_old = std::abs(rho_ptr[u]);
            int abs_new = std::abs(rho_ptr[u] + delta);
            
            if (abs_old > 4 && abs_new <= 4) d_viol--;
            else if (abs_old <= 4 && abs_new > 4) d_viol++;
            
            d_energy += (abs_new - abs_old);
            d_kurtosis += (abs_new * abs_new - abs_old * abs_old);
        }
        return {d_viol, d_energy, d_kurtosis};
    }

    // --- [v26 新功能] 雙點共軛翻轉評估 ---
    // 同時翻轉 p 和 (p+offset)%L
    inline EvalResult evaluate_pair_flip(int seq_idx, int p, int offset) const {
        // 為了效能，這裡我們模擬兩次單點翻轉的疊加
        // 注意：這是一個近似 (Approximation)，但在 Loop 中足夠精確且快
        
        EvalResult r1 = evaluate_flip(seq_idx, p);
        int p2 = (p + offset) % L;
        
        // 如果重疊，退化為不動 (翻兩次等於沒翻)
        if (p == p2) return {0, 0, 0};

        EvalResult r2 = evaluate_flip(seq_idx, p2);

        // 簡單疊加 (忽略兩點之間的交互作用項，這是為了速度的妥協)
        // 在大多數情況下，交互作用項只影響 lag=offset 的那一點
        return {r1.d_viol + r2.d_viol, r1.d_energy + r2.d_energy, r1.d_kurtosis + r2.d_kurtosis};
    }

    void apply_flip(int seq_idx, int p) {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        int limit = L; 
        for (int u = 1; u < limit; ++u) {
             sum_rho[u] += -2 * val_p * (center_ptr[u] + center_ptr[-u]);
        }
        int8_t new_val = -val_p;
        std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        seq[p] = new_val;
        update_single_buffer(seq_idx, p, new_val);
        update_metrics(); 
    }
    
    // 針對最大違規的踢擊
    void targeted_kick(XorShift128& rng) {
        if (max_violation_shift == 0) {
            mutate_block(rng, 4);
            return;
        }
        // 針對導致最大違規的間距進行破壞
        int u = max_violation_shift;
        int start = rng.next_int(L);
        // 翻轉一對間距為 u 的 bits
        int seq_idx = rng.next_int(2);
        apply_flip(seq_idx, start);
        apply_flip(seq_idx, (start + u) % L);
    }

    void mutate_block(XorShift128& rng, int size) {
        int start = rng.next_int(L);
        int target = rng.next_int(2);
        for(int k=0; k<size; ++k) apply_flip(target, (start+k)%L);
    }
};

void save_result(const std::string& base_dir, const SequenceState& st) {
    std::string sub_dir = std::to_string(st.L) + "_PACP";
    std::string full_dir = base_dir + "/" + sub_dir;
    std::string cmd = "mkdir -p \"" + full_dir + "\"";
    system(cmd.c_str()); 

    std::string filename = std::to_string(st.L) + "_solutions.txt";
    std::string full_path = full_dir + "/" + filename;
    
    int max_s = 0;
    for(int u=1; u<st.L; ++u) max_s = std::max(max_s, std::abs(st.sum_rho[u]));

    std::stringstream ss;
    ss << st.L << "," << max_s << ",";
    for(auto x : st.A) ss << (x > 0 ? "+" : "-");
    ss << ",";
    for(auto x : st.B) ss << (x > 0 ? "+" : "-");
    ss << "\n"; 

    std::ofstream outfile(full_path, std::ios::app);
    if (outfile.is_open()) {
        outfile.write(ss.str().c_str(), ss.str().length());
        outfile.close(); 
        std::cout << "\n[EVENT] NEW_SOL_SAVED L=" << st.L << " PSL=" << max_s << std::endl;
    }
}

// --- Dynamic Tabu ---
struct TabuManager {
    // 1. 先定義結構
    struct Entry { 
        int seq_idx; 
        int pos; 
    };

    // 2. 再宣告容器
    std::deque<Entry> list;
    size_t max_size;

    TabuManager(int size) : max_size((size_t)size) {}

    void add(int seq_idx, int pos) {
        Entry new_entry = {seq_idx, pos};
        list.push_back(new_entry);
        if (list.size() > max_size) list.pop_front();
    }

    // 3. 顯式指定類型，避免 auto 推導失敗亮紅線
    bool is_tabu(int seq_idx, int pos) const {
        for (const Entry& e : list) {
            if (e.seq_idx == seq_idx && e.pos == pos) return true;
        }
        return false;
    }

    void resize(int new_size) {
        max_size = (size_t)new_size;
        while(list.size() > max_size) list.pop_front();
    }

    void clear() { list.clear(); }
};

// --- Solver Process ---
void run_solver(int L, const std::string& out_dir, int worker_id) {
    std::cout << "[BOOT] Worker " << worker_id << " L=" << L << " (Dual-Flip Enabled)" << std::endl;
    
    unsigned int seed = std::random_device{}() + (worker_id * 777);
    XorShift128 rng(seed);
    
    SequenceState st(L);
    // 參數特化：L=44/46 需要稍長的 Tabu
    int base_tabu_size = std::max(6, L / 6);
    TabuManager tabu(base_tabu_size);

    auto reset_sequence = [&]() {
        st.randomize(rng);
        SeedRefiner::repair_seed_for_pqcp(st.A, st.B, L, rng);
        st.sync_buffers();
        st.full_recalc();
        tabu.clear();
    };

    reset_sequence();

    long long iter = 0, restarts = 0;
    int stuck = 0;
    // L=44/46 耐心要更高
    int patience_limit = L * 800; 

    while (true) {
        iter++;

        // 0. Success Check
        if (st.violations == 0) {
            if (st.is_strict_pqcp()) {
                save_result(out_dir, st);
                st.targeted_kick(rng); // 找到後，針對性破壞以尋找變體
                stuck = 0;
                tabu.clear();
                continue;
            }
        }

        // 1. Search Logic
        int best_seq = -1, best_p = -1;
        bool is_pair_move = false;
        int pair_offset = 0;

        int best_viol_diff = 10000;
        int best_energy_diff = 10000;
        int best_kurtosis_diff = -10000;

        bool shaping_mode = (st.violations == 0);
        int start_k = rng.next_int(L);

        // A. 嘗試單點翻轉 (Standard)
        for(int k=0; k<L; ++k) {
            int p = (start_k + k) % L;
            for (int seq_idx = 0; seq_idx < 2; ++seq_idx) {
                if (tabu.is_tabu(seq_idx, p)) continue;

                auto res = st.evaluate_flip(seq_idx, p);
                
                if (!shaping_mode) {
                    if (res.d_viol < best_viol_diff) {
                        best_viol_diff = res.d_viol; best_energy_diff = res.d_energy;
                        best_seq = seq_idx; best_p = p;
                        if (res.d_viol < 0) goto EXECUTE; 
                    } else if (res.d_viol == best_viol_diff && res.d_energy < best_energy_diff) {
                        best_energy_diff = res.d_energy; best_seq = seq_idx; best_p = p;
                    }
                } else {
                    if (res.d_viol > 0) continue;
                    if (res.d_kurtosis > best_kurtosis_diff) {
                        best_kurtosis_diff = res.d_kurtosis; best_seq = seq_idx; best_p = p;
                    }
                }
            }
        }

        // B. [強化] 如果單點沒路走 (stuck > L/2)，嘗試「針對性雙翻」
        // 只針對目前最大的 violation shift 進行修正嘗試
        if ((best_seq == -1 || best_viol_diff >= 0) && stuck > L/4 && st.max_violation_shift > 0) {
            int u = st.max_violation_shift;
            for(int k=0; k<L; ++k) {
                int p = (start_k + k) % L;
                for(int seq_idx=0; seq_idx<2; ++seq_idx) {
                    // 評估同時翻轉 p 和 p+u
                    auto res = st.evaluate_pair_flip(seq_idx, p, u);
                    
                    if (res.d_viol < best_viol_diff) {
                         best_viol_diff = res.d_viol; best_energy_diff = res.d_energy;
                         best_seq = seq_idx; best_p = p;
                         is_pair_move = true; pair_offset = u;
                         if (res.d_viol < 0) goto EXECUTE;
                    }
                }
            }
        }

        EXECUTE:
        bool accept = false;
        if (best_seq != -1) {
            if (!shaping_mode) {
                if (best_viol_diff < 0) accept = true;
                else if (best_viol_diff == 0 && best_energy_diff <= 0) accept = true;
                // 機率性接受平局，動態 Tabu 處理
                else if (best_viol_diff == 0 && rng.next_double() < 0.1) accept = true;
            } else {
                if (best_kurtosis_diff >= 0) accept = true;
                else if (rng.next_double() < 0.05) accept = true; 
            }
        }

        if (accept) {
            st.apply_flip(best_seq, best_p);
            tabu.add(best_seq, best_p);
            
            if (is_pair_move) {
                int p2 = (best_p + pair_offset) % L;
                st.apply_flip(best_seq, p2);
                tabu.add(best_seq, p2); // 雙點都入 Tabu
            }

            stuck = 0;
            // 成功移動時，Tabu 縮小 (變得靈活)
            if (tabu.max_size > (size_t)base_tabu_size) tabu.resize(base_tabu_size);
        } else {
            stuck++;
            // 停滯時，Tabu 變大 (強迫探索遠方)
            if (stuck % 50 == 0) tabu.resize(tabu.max_size + 1);
        }

        // 3. Stagnation / Kick
        if (stuck > patience_limit) {
            // Targeted Kick: 專門破壞最大旁瓣
            st.targeted_kick(rng);
            tabu.clear();
            tabu.resize(base_tabu_size);
            stuck = 0;
            
            if (iter % (patience_limit * 10) == 0) {
                restarts++;
                reset_sequence();
            }
        }

        if (iter % 5000 == 0) {
             std::cout << "[STAT] Iter=" << iter << " Rst=" << restarts << " Viol=" << st.violations << " MX=" << st.max_violation_shift << "\r" << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    if (argc < 4) return 1;
    std::cout.setf(std::ios::unitbuf); 
    run_solver(std::stoi(argv[1]), argv[2], std::stoi(argv[3]));
    return 0;
}