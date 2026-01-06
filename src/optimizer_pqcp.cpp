/*
   PQCP Optimizer v23.2 - Atomic Write & Hardware Safe
   Integration: lib/pqcp_tuner.h
   Features: 
     - Single-Shot Write (Prevents text scrambling)
     - Strict Hardware Protection (No junk writes)
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <string>
#include <sstream> // 必備：用於記憶體組字串
#include <cstdlib>
#include "../lib/pqcp_tuner.h" 

// --- 輕量級 RNG ---
struct XorShift128 {
    uint32_t x, y, z, w;
    XorShift128(uint32_t seed) {
        x = seed; y = 362436069; z = 521288629; w = 88675123;
        for(int i=0; i<20; ++i) next();
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
    std::vector<int> sum_rho;
    int violations = 0; 

    SequenceState(int length) : L(length) {
        A.resize(L); B.resize(L);
        sum_rho.resize(L);
    }

    void randomize(XorShift128& rng) {
        for(int i=0; i<L; ++i) {
            A[i] = (rng.next() & 1) ? 1 : -1;
            B[i] = (rng.next() & 1) ? 1 : -1;
        }
    }

    void full_recalc() {
        std::fill(sum_rho.begin(), sum_rho.end(), 0);
        for (int u = 0; u < L; ++u) {
            int r = 0;
            for (int i = 0; i < L; ++i) {
                r += A[i] * A[(i + u) % L] + B[i] * B[(i + u) % L];
            }
            sum_rho[u] = r;
        }
        update_metrics();
    }

    void update_metrics() {
        violations = 0;
        int limit = L / 2;
        for (int u = 1; u <= limit; ++u) {
            if (std::abs(sum_rho[u]) > 4) violations++;
        }
    }

    // 嚴格檢查 (Strict PQCP)
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

    // 增量評估
    struct MoveResult { int d_viol; };
    inline MoveResult evaluate_flip(int seq_idx, int p) const {
        const std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        int val_p = seq[p];
        int d_viol = 0;
        int limit = L >> 1;

        for (int u = 1; u <= limit; ++u) {
            int p_plus = p + u; if (p_plus >= L) p_plus -= L;
            int p_minus = p - u; if (p_minus < 0) p_minus += L;
            int delta = -2 * val_p * (seq[p_plus] + seq[p_minus]);
            if (delta == 0) continue;

            int old_abs = std::abs(sum_rho[u]);
            int new_abs = std::abs(sum_rho[u] + delta);
            
            if (old_abs > 4 && new_abs <= 4) d_viol--;
            else if (old_abs <= 4 && new_abs > 4) d_viol++;
        }
        return {d_viol};
    }

    void apply_flip(int seq_idx, int p) {
        std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        int val_p = seq[p];
        int limit = L; 
        for (int u = 1; u < limit; ++u) {
             int p_plus = p + u; if (p_plus >= L) p_plus -= L;
             int p_minus = p - u; if (p_minus < 0) p_minus += L;
             int delta = -2 * val_p * (seq[p_plus] + seq[p_minus]);
             sum_rho[u] += delta;
        }
        seq[p] = -val_p; 
        update_metrics(); 
    }
    
    void mutate_block(XorShift128& rng, int size) {
        int start = rng.next_int(L);
        int target = rng.next_int(2);
        for(int k=0; k<size; ++k) {
            apply_flip(target, (start+k)%L);
        }
    }
};

// --- [關鍵優化] 安全且高效的寫入 ---
void save_result(const std::string& base_dir, const SequenceState& st) {
    // 1. 準備路徑 (只做一次 system call)
    std::string sub_dir = std::to_string(st.L) + "_PACP";
    std::string full_dir = base_dir + "/" + sub_dir;
    
    // mkdir -p 即使存在也不會報錯，消耗極低
    std::string cmd = "mkdir -p \"" + full_dir + "\"";
    system(cmd.c_str()); 

    std::string filename = std::to_string(st.L) + "_solutions.txt";
    std::string full_path = full_dir + "/" + filename;
    
    // 2. 記憶體組裝字串 (避免多次 I/O 導致插隊)
    int max_s = 0;
    for(int u=1; u<st.L; ++u) max_s = std::max(max_s, std::abs(st.sum_rho[u]));

    std::stringstream ss;
    ss << st.L << "," << max_s << ",";
    for(auto x : st.A) ss << (x > 0 ? "+" : "-");
    ss << ",";
    for(auto x : st.B) ss << (x > 0 ? "+" : "-");
    ss << "\n"; // 確保換行

    // 3. 單次原子寫入 (Atomic Append)
    // 使用 std::ios::app 確保永遠寫在檔案尾端
    std::ofstream outfile(full_path, std::ios::app);
    if (outfile.is_open()) {
        outfile.write(ss.str().c_str(), ss.str().length());
        outfile.close(); // 立即關閉釋放資源
        
        // 通知 UI
        std::cout << "\n[EVENT] NEW_SOL_SAVED L=" << st.L << " PSL=" << max_s << std::endl;
    }
}

// --- 演算法主流程 ---
void run_solver(int L, const std::string& out_dir, int worker_id) {
    std::cout << "[BOOT] Worker " << worker_id << " Ready." << std::endl;
    
    unsigned int seed = std::random_device{}() + (worker_id * 77777);
    XorShift128 rng(seed);
    SequenceState st(L);
    SolverConfig cfg(L); 

    // 種子修復
    st.randomize(rng);
    SeedRefiner::repair_seed_for_pqcp(st.A, st.B, L, rng);
    st.full_recalc();

    long long iter = 0, restarts = 0;
    int stuck = 0;

    while (true) {
        iter++;

        // Gatekeeper
        if (st.violations == 0) {
            if (st.is_strict_pqcp()) {
                save_result(out_dir, st);
                // 找到後繼續找，破壞 50% 結構
                st.mutate_block(rng, L/2); 
                stuck = 0;
                continue;
            }
        }

        // Hill Climbing
        int best_seq = -1, best_p = -1;
        int best_viol = 9999;
        int start_k = rng.next_int(L);

        for(int k=0; k<L; ++k) {
            int p = (start_k + k) % L;
            
            auto resA = st.evaluate_flip(0, p);
            if (resA.d_viol < best_viol) { best_viol = resA.d_viol; best_seq=0; best_p=p; }
            if (resA.d_viol < 0) goto APPLY;

            auto resB = st.evaluate_flip(1, p);
            if (resB.d_viol < best_viol) { best_viol = resB.d_viol; best_seq=1; best_p=p; }
            if (resB.d_viol < 0) goto APPLY;
        }

        if (best_viol == 0 && best_seq != -1 && rng.next_double() < 0.1) goto APPLY;
        
        stuck++; 
        goto CHECK_RESET;

        APPLY:
        st.apply_flip(best_seq, best_p);
        stuck = 0;

        CHECK_RESET:
        if (stuck > cfg.stagnation_limit) {
            st.mutate_block(rng, cfg.block_size);
            stuck = 0;
            if (iter % (cfg.stagnation_limit * 20) == 0) {
                restarts++;
                st.randomize(rng);
                SeedRefiner::repair_seed_for_pqcp(st.A, st.B, L, rng);
                st.full_recalc();
            }
        }

        // 高頻回報 (防止 UI 卡死)
        if (iter % 5000 == 0) {
             std::cout << "[STAT] Iter=" << iter << " Rst=" << restarts << " Viol=" << st.violations << "\r" << std::flush;
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1;
    std::cout.setf(std::ios::unitbuf); // 無緩衝輸出
    run_solver(std::stoi(argv[1]), argv[2], std::stoi(argv[3]));
    return 0;
}