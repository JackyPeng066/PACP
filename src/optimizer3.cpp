/*
   PACP Optimizer v12.0 - Final Artifact
   Usage: ./optimizer_csp <L> <OutDir> <WorkerID>
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

// --- RNG ---
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
};

// --- State ---
class SequenceState {
public:
    int L;
    std::vector<int8_t> A, B;
    std::vector<int> rho_A, rho_B, sum_rho;
    std::vector<int> bad_k_list;
    int bad_count = 0;

    SequenceState(int length) : L(length) {
        A.resize(L); B.resize(L);
        rho_A.resize(L); rho_B.resize(L); sum_rho.resize(L);
    }

    void randomize(XorShift128& rng, bool symmetric_start) {
        for(int i=0; i<L; ++i) {
            A[i] = (rng.next() & 1) ? 1 : -1;
            B[i] = (rng.next() & 1) ? 1 : -1;
        }
        if (symmetric_start) {
            int half = (L + 1) / 2;
            for(int i=0; i<half; ++i) {
                A[L - 1 - i] = A[i]; 
                B[L - 1 - i] = B[i]; 
            }
        }
        full_recalc();
    }

    void full_recalc() {
        std::fill(rho_A.begin(), rho_A.end(), 0);
        std::fill(rho_B.begin(), rho_B.end(), 0);
        for (int u = 0; u < L; ++u) {
            for (int i = 0; i < L; ++i) {
                rho_A[u] += A[i] * A[(i + u) % L];
                rho_B[u] += B[i] * B[(i + u) % L];
            }
            sum_rho[u] = rho_A[u] + rho_B[u];
        }
        update_metrics();
    }

    void update_metrics() {
        bad_k_list.clear();
        int check_limit = L / 2; 
        for (int u = 1; u <= check_limit; ++u) {
            if (std::abs(sum_rho[u]) > 4) bad_k_list.push_back(u);
        }
        bad_count = bad_k_list.size();
    }

    struct MoveResult { int d_bad; long long d_viol; };

    // Delta update logic
    MoveResult evaluate_flip(int seq_idx, int p) const {
        const std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        int val_p = seq[p];
        int d_bad = 0;
        long long d_viol = 0;
        int check_limit = L / 2;

        for (int u = 1; u <= check_limit; ++u) {
            int p_plus = (p + u); if (p_plus >= L) p_plus -= L;
            int p_minus = (p - u); if (p_minus < 0) p_minus += L;
            int delta = -2 * val_p * (seq[p_plus] + seq[p_minus]);
            if (delta == 0) continue; 

            int old_abs = std::abs(sum_rho[u]);
            int new_abs = std::abs(sum_rho[u] + delta);

            bool was_bad = (old_abs > 4);
            bool is_bad = (new_abs > 4);

            if (was_bad && !is_bad) d_bad--;
            if (!was_bad && is_bad) d_bad++;
            
            // Heuristic tie-breaker: reduce total violation magnitude
            int old_excess = was_bad ? (old_abs - 4) : 0;
            int new_excess = is_bad ? (new_abs - 4) : 0;
            d_viol += (new_excess - old_excess);
        }
        return {d_bad, d_viol};
    }

    void apply_flip(int seq_idx, int p) {
        std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        std::vector<int>& rho = (seq_idx == 0) ? rho_A : rho_B;
        int val_p = seq[p];
        for (int u = 1; u < L; ++u) {
             int p_plus = (p + u); if (p_plus >= L) p_plus -= L;
             int p_minus = (p - u); if (p_minus < 0) p_minus += L;
             int delta = -2 * val_p * (seq[p_plus] + seq[p_minus]);
             rho[u] += delta;
             sum_rho[u] += delta;
        }
        seq[p] = -val_p; 
        update_metrics(); 
    }
};

// --- Save Logic ---
void save_result(const std::string& out_dir, const SequenceState& st) {
    std::random_device rd;
    std::stringstream ss;
    
    // Check max PSL just to be sure
    int max_s = 0;
    for(int u=1; u<=st.L/2; ++u) max_s = std::max(max_s, std::abs(st.sum_rho[u]));
    
    // Clean path string
    std::string clean_dir = out_dir;
    if (!clean_dir.empty() && clean_dir.back() == '/') clean_dir.pop_back();

    if (!fs::exists(clean_dir)) fs::create_directories(clean_dir);

    ss << clean_dir << "/L" << st.L << "_PSL" << max_s << "_" << std::hex << rd() << ".csv";
    
    std::ofstream outfile(ss.str());
    if (outfile.is_open()) {
        outfile << st.L << "," << max_s << ",";
        for(auto x : st.A) outfile << (x==1 ? '+' : '-'); 
        outfile << ",";
        for(auto x : st.B) outfile << (x==1 ? '+' : '-');
        outfile << "\n";
        outfile.close();
        std::cout << "[SYSTEM] Saved: " << ss.str() << std::endl;
    }
}

// --- Solver Loop ---
void run_solver(int L, const std::string& out_dir, int worker_id) {
    unsigned int seed = std::random_device{}() + (worker_id * 7777);
    XorShift128 rng(seed);

    int COARSE_FLIP_NUM = std::max(2, L / 12); 
    int FINE_TUNE_LIMIT = L * 25;              
    
    SequenceState st(L);
    long long iteration = 0;
    long long restarts = 0;

    std::cout << "[INIT] Worker=" << worker_id << " L=" << L << std::endl;

    while (true) {
        restarts++;
        // 50% Sym start (helps initialization), 50% Random start
        st.randomize(rng, (rng.next_int(2) == 0));

        int stuck = 0;
        int fine_counter = 0;
        
        while (stuck < 800) { 
            iteration++;
            
            // Victory Check
            if (st.bad_count == 0) {
                std::cout << "[EVENT] Action=VICTORY Worker=" << worker_id << std::endl;
                save_result(out_dir, st);
                return; // Stop worker on success
            }

            if (st.bad_k_list.empty()) { st.update_metrics(); continue; }
            
            bool move_made = false;
            int start_i = rng.next_int(L); 
            
            // Scan for targeted repairs
            for (int scan = 0; scan < L; ++scan) {
                int i = (start_i + scan) % L;
                
                // Check A
                auto resA = st.evaluate_flip(0, i);
                if (resA.d_bad < 0 || (resA.d_bad == 0 && resA.d_viol < 0)) {
                    st.apply_flip(0, i);
                    move_made = true;
                    if (st.bad_count <= 2) std::cout << "[EVENT] Action=Dive BadK=" << st.bad_count << std::endl;
                    stuck = 0;
                    break; 
                }
                
                // Check B
                auto resB = st.evaluate_flip(1, i);
                if (resB.d_bad < 0 || (resB.d_bad == 0 && resB.d_viol < 0)) {
                    st.apply_flip(1, i);
                    move_made = true;
                    if (st.bad_count <= 2) std::cout << "[EVENT] Action=Dive BadK=" << st.bad_count << std::endl;
                    stuck = 0;
                    break;
                }
            }
            
            if (move_made) {
                fine_counter++;
            } else {
                // Kick strategy
                for(int f=0; f<COARSE_FLIP_NUM; ++f) st.apply_flip(rng.next_int(2), rng.next_int(L));
                
                if (stuck % 100 == 0 && st.bad_count < 8) {
                     std::cout << "[EVENT] Action=KICK BadK=" << st.bad_count << std::endl;
                }
                stuck++;
            }
            
            // Heartbeat (~0.5s)
            if (iteration % 50000 == 0) {
                std::cout << "[STAT] Iter=" << iteration 
                          << " Restarts=" << restarts 
                          << " BadK=" << st.bad_count 
                          << std::endl;
            }
            
            if (fine_counter > FINE_TUNE_LIMIT) break; 
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1;
    int L = std::stoi(argv[1]);
    std::string out_dir = argv[2];
    int worker_id = std::stoi(argv[3]);
    
    std::cout.setf(std::ios::unitbuf);
    run_solver(L, out_dir, worker_id);
    return 0;
}