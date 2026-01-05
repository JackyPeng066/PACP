/*
   PACP Optimizer v15.1 - Heavyweight Edition (Cleaned)
   Target: L >= 110
   Strategy: Block Mutation + Plateau Search
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
    inline double next_double() { return (double)next() / 4294967296.0; }
};

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

    void randomize(XorShift128& rng) {
        for(int i=0; i<L; ++i) {
            A[i] = (rng.next() & 1) ? 1 : -1;
            B[i] = (rng.next() & 1) ? 1 : -1;
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

    // --- Special: Block Randomization ---
    // [FIXED] Removed unused 'seq' variable
    void apply_block_mutation(int seq_idx, int start_idx, int len, XorShift128& rng) {
        for(int k=0; k<len; ++k) {
            int p = (start_idx + k) % L;
            // 50% chance to flip this bit
            if (rng.next_int(2) == 0) {
                apply_flip(seq_idx, p);
            }
        }
    }
};

void save_result(const std::string& out_dir, const SequenceState& st) {
    std::random_device rd;
    std::stringstream ss;
    int max_s = 0;
    for(int u=1; u<=st.L/2; ++u) max_s = std::max(max_s, std::abs(st.sum_rho[u]));
    
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
        outfile.flush();
        outfile.close();
        std::cout << "[SYSTEM] Saved: " << ss.str() << std::endl;
    }
}

void run_solver(int L, const std::string& out_dir, int worker_id) {
    unsigned int seed = std::random_device{}() + (worker_id * 9999);
    XorShift128 rng(seed);

    int BLOCK_SIZE = std::max(4, L / 10); 
    int STUCK_LIMIT = 2000; 

    SequenceState st(L);
    long long iteration = 0;
    long long restarts = 0;

    std::cout << "[INIT] Heavy Worker=" << worker_id << " L=" << L << " BlockSize=" << BLOCK_SIZE << std::endl;

    while (true) {
        restarts++;
        st.randomize(rng); 

        int stuck = 0;
        
        while (stuck < STUCK_LIMIT) { 
            iteration++;
            
            if (st.bad_count == 0) {
                std::cout << "[EVENT] Action=VICTORY Worker=" << worker_id << std::endl;
                save_result(out_dir, st);
                return; 
            }

            // --- Plateau Search Strategy ---
            bool move_made = false;
            int start_i = rng.next_int(L); 
            
            for (int scan = 0; scan < L; ++scan) {
                int i = (start_i + scan) % L;
                
                // Check A
                auto resA = st.evaluate_flip(0, i);
                bool acceptA = (resA.d_bad < 0) || 
                               (resA.d_bad == 0 && resA.d_viol < 0) ||
                               (resA.d_bad == 0 && resA.d_viol == 0 && rng.next_double() < 0.15); 

                if (acceptA) {
                    st.apply_flip(0, i);
                    move_made = true;
                    if (resA.d_bad < 0) stuck = 0; 
                    if (st.bad_count <= 2 && resA.d_bad < 0) std::cout << "[EVENT] Action=Dive BadK=" << st.bad_count << std::endl;
                    break; 
                }
                
                // Check B
                auto resB = st.evaluate_flip(1, i);
                bool acceptB = (resB.d_bad < 0) || 
                               (resB.d_bad == 0 && resB.d_viol < 0) ||
                               (resB.d_bad == 0 && resB.d_viol == 0 && rng.next_double() < 0.15);

                if (acceptB) {
                    st.apply_flip(1, i);
                    move_made = true;
                    if (resB.d_bad < 0) stuck = 0;
                    if (st.bad_count <= 2 && resB.d_bad < 0) std::cout << "[EVENT] Action=Dive BadK=" << st.bad_count << std::endl;
                    break;
                }
            }
            
            // --- Block Mutation Strategy ---
            if (!move_made) {
                stuck++;
                if (stuck % 20 == 0) {
                    int target_seq = rng.next_int(2);
                    int start_pos = rng.next_int(L);
                    st.apply_block_mutation(target_seq, start_pos, BLOCK_SIZE, rng);
                    
                    if (stuck % 200 == 0) {
                         std::cout << "[EVENT] Action=BLOCK_MUTATE Pos=" << start_pos << " BadK=" << st.bad_count << std::endl;
                    }
                }
            } else {
                if (st.bad_count > 4) stuck++; 
            }
            
            if (iteration % 50000 == 0) {
                std::cout << "[STAT] Iter=" << iteration 
                          << " Restarts=" << restarts 
                          << " BadK=" << st.bad_count 
                          << std::endl;
            }
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