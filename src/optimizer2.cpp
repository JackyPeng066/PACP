/*
   Goal 2 Dedicated Optimizer - Engineering Edition
   File: src/optimizer2.cpp
   Fix: Replaced system() calls with std::filesystem for Windows/Linux compatibility.
*/

#include <iostream>
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>
#include <fstream>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <thread>
#include <filesystem>

// Namespace alias for cleaner code
namespace fs = std::filesystem;

// --- 1. XorShift256 ---
struct XorShift256 {
    uint64_t s[4];
    XorShift256(uint64_t seed) {
        uint64_t z = seed + 0x9E3779B97F4A7C15ULL;
        for (int i = 0; i < 4; ++i) {
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            s[i] = z ^ (z >> 31);
        }
    }
    inline uint64_t next() {
        const uint64_t result = s[0] + s[3];
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3]; s[2] ^= t;
        s[3] = (s[3] << 45) | (s[3] >> 19);
        return result;
    }
    inline int next_int(int range) { return next() % range; }
    inline double next_double() { return (next() >> 11) * 0x1.0p-53; }
};

// --- 2. Simple Tabu ---
struct SimpleTabu {
    std::vector<int> data;
    size_t idx = 0;
    SimpleTabu(int size) { data.resize(size, -1); }
    void add(int p) { data[idx] = p; idx = (idx + 1) % data.size(); }
    bool contains(int p) const { 
        for(int v : data) if(v == p) return true; 
        return false; 
    }
    void clear() { std::fill(data.begin(), data.end(), -1); }
};

// --- 3. Sequence State ---
class SequenceState {
public:
    int L, half_L;
    std::vector<int8_t> A, B; 
    std::vector<int8_t> ext_A, ext_B; 
    std::vector<int> sum_rho;         
    
    int zcz_violations = 0;
    int mid_val = 0;        

    SequenceState(int length) : L(length), half_L(length/2) {
        A.resize(L); B.resize(L);
        ext_A.resize(3 * L); ext_B.resize(3 * L);
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

    void randomize(XorShift256& rng) {
        for(int i=0; i<L; ++i) { A[i] = (rng.next()&1)?1:-1; B[i] = (rng.next()&1)?1:-1; }
        sync_buffers();
    }

    void full_recalc() {
        std::fill(sum_rho.begin(), sum_rho.end(), 0);
        const int8_t* ptrA = ext_A.data() + L;
        const int8_t* ptrB = ext_B.data() + L;
        
        for (int u = 0; u < L; ++u) {
            int r = 0;
            #pragma GCC ivdep
            for (int i = 0; i < L; ++i) r += ptrA[i] * ptrA[i + u] + ptrB[i] * ptrB[i + u];
            sum_rho[u] = r;
        }
        update_metrics();
    }

    void update_metrics() {
        zcz_violations = 0; 
        for (int u = 1; u < half_L; ++u) {
            if (sum_rho[u] != 0) zcz_violations++;
        }
        mid_val = std::abs(sum_rho[half_L]);
    }

    struct ScoreDesc { int d_viol; int d_energy; };

    inline ScoreDesc evaluate_move(int seq_idx, int p) const {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        const int* rho_ptr = sum_rho.data();
        
        int d_viol = 0; 
        int d_energy = 0;

        for (int u = 1; u < half_L; ++u) {
            int sum_neighbors = center_ptr[u] + center_ptr[-u]; 
            if (sum_neighbors == 0) continue; 
            
            int delta = -2 * val_p * sum_neighbors;
            int abs_old = std::abs(rho_ptr[u]);
            int abs_new = std::abs(rho_ptr[u] + delta);
            
            if (abs_old == abs_new) continue;

            if (abs_old > 0 && abs_new == 0) d_viol--;      
            else if (abs_old == 0 && abs_new > 0) d_viol++; 
            
            d_energy += (abs_new - abs_old);
        }
        return {d_viol, d_energy};
    }

    void apply_flip(int seq_idx, int p) {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        
        for (int u = 1; u < L; ++u) {
            sum_rho[u] += -2 * val_p * (center_ptr[u] + center_ptr[-u]);
        }
        
        int8_t new_val = -val_p;
        std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        seq[p] = new_val;
        update_single_buffer(seq_idx, p, new_val);
        update_metrics(); 
    }

    void mutate(XorShift256& rng, int strength) {
        for(int k=0; k<strength; ++k) apply_flip(rng.next_int(2), rng.next_int(L));
    }
};

// --- 4. Path Management (Fixed: No system() calls) ---
struct PathManager {
    std::string opt_file, szcp_file, near_file, log_file;
    
    PathManager(std::string root_dir, int L, int worker_id) {
        // Native C++ directory creation (Cross-platform, safe)
        try {
            fs::path base_path = fs::path(root_dir) / std::to_string(L);
            fs::path worker_path = base_path / ("worker_" + std::to_string(worker_id));
            
            // Create directories recursively
            if (!fs::exists(worker_path)) {
                fs::create_directories(worker_path);
            }

            opt_file  = (base_path / "Goal2_Opt.txt").string();
            szcp_file = (base_path / "Goal2_SZCP.txt").string();
            near_file = (base_path / "Goal2_Near.txt").string();
            log_file  = (worker_path / "status.log").string();
            
        } catch (const std::exception& e) {
            std::cerr << "Filesystem Error: " << e.what() << std::endl;
        }
    }

    void save(const SequenceState& st, const char* type) {
        std::string target;
        if (std::strcmp(type, "OPT") == 0) target = opt_file;
        else if (std::strcmp(type, "SZCP") == 0) target = szcp_file;
        else target = near_file;

        std::ofstream outfile(target, std::ios::app);
        if (outfile.is_open()) {
            outfile << type << ",L=" << st.L 
                    << ",Mid=" << st.mid_val << ",";
            
            for(auto x : st.A) outfile << (x > 0 ? "+" : "-");
            outfile << ",";
            for(auto x : st.B) outfile << (x > 0 ? "+" : "-");
            outfile << "\n";
        }
    }

    void update_status(long long iter, int rst, int viol, int mid, long long found, double elapsed) {
        std::ofstream stat(log_file, std::ios::trunc);
        if (stat.is_open()) {
            stat << "Iter: " << iter << " | Rst: " << rst 
                 << " | ZCZ_Viol: " << viol << " | Mid: " << mid 
                 << " | Found: " << found << " | Time: " << (long)elapsed << "s\n";
        }
    }
};

// --- 5. Main Solver ---
void run_solver(int L, std::string results_root, int wid, long long time_limit) {
    if (L % 2 != 0) {
        std::cerr << "Error: Length must be even for Goal 2." << std::endl;
        return;
    }

    uint64_t seed = (uint64_t)wid * 0x5851F42D4C957F2D + 
                    (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    XorShift256 rng(seed);
    SequenceState st(L);
    PathManager paths(results_root, L, wid);
    SimpleTabu tabu(std::max(4, L/8));

    int SMALL_KICK = L * 20; 
    int BIG_KICK   = L * 200;
    int RESTART    = L * 3000;
    
    long long iter = 0, found = 0, restarts = 0;
    int stuck = 0, total_stuck = 0;
    auto start_time = std::chrono::steady_clock::now();

    auto full_restart = [&]() {
        st.randomize(rng); st.full_recalc();
        stuck = 0; total_stuck = 0; tabu.clear(); restarts++;
    };

    full_restart();
    const int SLEEP_BATCH = 4096; 

    while (true) {
        iter++;
        
        if (time_limit > 0 && (iter % 10000 == 0)) {
            double elap = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
            if (elap >= time_limit) break;
        }

        if ((iter & (SLEEP_BATCH-1)) == 0) std::this_thread::sleep_for(std::chrono::nanoseconds(1));

        if (st.zcz_violations == 0) {
            if (st.mid_val == 4) {
                found++; paths.save(st, "OPT");
                st.mutate(rng, std::max(4, L/3)); stuck=0; tabu.clear(); continue;
            } 
            else if (st.mid_val == 2) {
                found++; paths.save(st, "SZCP");
                st.mutate(rng, std::max(4, L/3)); stuck=0; tabu.clear(); continue;
            }
            else if (st.mid_val > 4) {
                if (rng.next_double() < 0.1) paths.save(st, "NEAR");
                st.mutate(rng, 2); stuck=0; tabu.clear(); continue;
            }
        }

        int best_seq = -1, best_p = -1;
        int checks = std::max(10, (int)(L * 0.5)); 
        int start_k = rng.next_int(L);
        int best_viol = 1000, best_en = 1000;

        for(int k=0; k<checks; ++k) {
            int p = (start_k + k) % L;
            int seq = (k & 1); 
            if (tabu.contains(p)) continue;

            auto res = st.evaluate_move(seq, p);

            if (res.d_viol < best_viol) {
                best_viol = res.d_viol; best_en = res.d_energy; best_seq = seq; best_p = p;
                if (best_viol < 0) break;
            } else if (res.d_viol == best_viol && res.d_energy < best_en) {
                best_en = res.d_energy; best_seq = seq; best_p = p;
            }
        }

        bool accept = false;
        if (best_seq != -1) {
            if (best_viol < 0) accept = true;
            else if (best_viol == 0 && best_en <= 0) accept = true;
            else if (best_viol == 0 && rng.next_double() < 0.05) accept = true; 
        }

        if (accept) {
            st.apply_flip(best_seq, best_p); tabu.add(best_p); stuck = 0;
        } else { stuck++; total_stuck++; }

        if (stuck > SMALL_KICK) { st.mutate(rng, 2 + rng.next_int(3)); stuck = 0; tabu.clear(); }
        if (total_stuck > BIG_KICK) { st.mutate(rng, std::max(6, L/4)); total_stuck = 0; tabu.clear(); }
        if (total_stuck > RESTART) full_restart();

        if ((iter & 32767) == 0) {
            double elap = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
            paths.update_status(iter, restarts, st.zcz_violations, st.mid_val, found, elap);
        }
    }
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    if (argc < 5) return 1;
    run_solver(std::stoi(argv[1]), argv[2], std::stoi(argv[3]), std::stoll(argv[4]));
    return 0;
}