/*
   PQCP Optimizer v38.0 - STABLE & COOL
   Target: L=44, 46, 58
   Features:
     - Aggressive CPU Cooling (Sleep every 2048 iters)
     - Reduced I/O Frequency (Prevents shell read errors)
     - Strict Peak Logic
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
#include <cstring>
#include <chrono>
#include <iomanip>
#include <cstdint>
#include <thread>
#include <filesystem>
#include <deque>
#include "../lib/pqcp_tuner.h" 

// --- RNG ---
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
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = (s[3] << 45) | (s[3] >> 19);
        return result;
    }
    inline int next_int(int range) { return next() % range; }
    inline double next_double() { return (next() >> 11) * 0x1.0p-53; }
};

// --- Tabu ---
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

// --- Sequence State ---
class SequenceState {
public:
    int L;
    std::vector<int8_t> A, B; 
    std::vector<int8_t> ext_A, ext_B; 
    std::vector<int> sum_rho;
    
    int violations = 0; 
    int peak_count = 0; 
    int max_sidelobe = 0;
    long long total_energy = 0;

    SequenceState(int length) : L(length) {
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
        double r = rng.next_double();
        if (r < 0.3) { 
            for(int i=0; i<L; ++i) {
                if(i%2==0) { A[i] = (rng.next()&1)?1:-1; B[i] = (rng.next()&1)?1:-1; }
                else { A[i] = A[i-1]; B[i] = -B[i-1]; }
            }
        } else { 
            for(int i=0; i<L; ++i) { A[i] = (rng.next()&1)?1:-1; B[i] = (rng.next()&1)?1:-1; }
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
            for (int i = 0; i < L; ++i) r += ptrA[i] * ptrA[i + u] + ptrB[i] * ptrB[i + u];
            sum_rho[u] = r;
        }
        update_metrics();
    }

    void update_metrics() {
        violations = 0; peak_count = 0; max_sidelobe = 0; total_energy = 0;
        int limit = L / 2;
        for (int u = 1; u <= limit; ++u) {
            int val = std::abs(sum_rho[u]);
            if (val > max_sidelobe) max_sidelobe = val;
            int weight = (u == limit && L % 2 == 0) ? 1 : 2;
            total_energy += val * weight;
            if (val > 4) violations += weight;
            if (val > 0) peak_count += weight;
        }
    }

    struct ScoreDesc { int d_viol; int d_energy; };
    inline ScoreDesc evaluate_descent(int seq_idx, int p) const {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        int limit = L >> 1;
        const int* rho_ptr = sum_rho.data();
        int d_viol = 0; int d_energy = 0;

        for (int u = 1; u <= limit; ++u) {
            int sum_neighbors = center_ptr[u] + center_ptr[-u]; 
            if (sum_neighbors == 0) continue; 
            int delta = -2 * val_p * sum_neighbors;
            int abs_old = std::abs(rho_ptr[u]);
            int abs_new = std::abs(rho_ptr[u] + delta);
            if (abs_old == abs_new) continue;
            int weight = (u == limit && L % 2 == 0) ? 1 : 2;

            if (abs_old > 4 && abs_new <= 4) d_viol -= weight;
            else if (abs_old <= 4 && abs_new > 4) d_viol += weight;
            d_energy += (abs_new - abs_old) * weight;
        }
        return {d_viol, d_energy};
    }

    struct ScoreShape { int d_peaks; int d_energy; bool valid; };
    inline ScoreShape evaluate_shaping(int seq_idx, int p) const {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        int limit = L >> 1;
        const int* rho_ptr = sum_rho.data();
        int d_peaks = 0; int d_energy = 0;

        for (int u = 1; u <= limit; ++u) {
            int sum_neighbors = center_ptr[u] + center_ptr[-u]; 
            if (sum_neighbors == 0) continue; 
            int delta = -2 * val_p * sum_neighbors;
            int abs_old = std::abs(rho_ptr[u]);
            int abs_new = std::abs(rho_ptr[u] + delta);
            if (abs_new > 4) return {0, 0, false}; 
            int weight = (u == limit && L % 2 == 0) ? 1 : 2;

            if (abs_old > 0 && abs_new == 0) d_peaks -= weight;
            else if (abs_old == 0 && abs_new > 0) d_peaks += weight;
            d_energy += (abs_new - abs_old) * weight;
        }
        return {d_peaks, d_energy, true};
    }

    void apply_flip(int seq_idx, int p) {
        const int8_t* center_ptr = (seq_idx == 0) ? (ext_A.data() + L + p) : (ext_B.data() + L + p);
        int val_p = *center_ptr;
        int limit = L; 
        for (int u = 1; u < limit; ++u) sum_rho[u] += -2 * val_p * (center_ptr[u] + center_ptr[-u]);
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

// --- Path Management ---
struct PathManager {
    std::string sol_file, near_file, status_file;
    PathManager(std::string root, int L, int worker_id) {
        std::string main_dir = root + "/" + std::to_string(L) + "_PACP";
        std::string worker_dir = main_dir + "/Workers/Worker_" + std::to_string(worker_id);
        std::string cmd = "mkdir -p \"" + worker_dir + "\"";
        system(cmd.c_str());
        sol_file = main_dir + "/" + std::to_string(L) + "_PQCP.txt";
        near_file = main_dir + "/" + std::to_string(L) + "_near.txt";
        status_file = worker_dir + "/status.txt";
    }
    void save(const SequenceState& st, bool is_strict) {
        std::string target = is_strict ? sol_file : near_file;
        std::ofstream outfile(target, std::ios::app);
        if (outfile.is_open()) {
            outfile << (is_strict ? "PQCP" : "NEAR") << ",L=" << st.L 
                    << ",Max=" << st.max_sidelobe 
                    << ",Peaks=" << st.peak_count << ",";
            for(auto x : st.A) outfile << (x > 0 ? "+" : "-");
            outfile << ",";
            for(auto x : st.B) outfile << (x > 0 ? "+" : "-");
            outfile << "\n";
        }
    }
    void update_dashboard(long long iter, int rst, int viol, int peaks, long long found) {
        std::ofstream stat(status_file, std::ios::trunc);
        if (stat.is_open()) {
            stat << "Iter=" << iter << " Rst=" << rst << " Viol=" << viol 
                 << " Peaks=" << peaks << " Sol=" << found << "\n";
        }
    }
};

void run_solver(int L, const std::string& out_dir, int worker_id) {
    uint64_t seed_val = (uint64_t)worker_id * 0x5851F42D4C957F2D + 
                        (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    XorShift256 rng(seed_val);
    SequenceState st(L);
    PathManager paths(out_dir, L, worker_id);
    SimpleTabu tabu(std::max(4, L/8));

    int SMALL_KICK_LIMIT = L * 20; 
    int BIG_KICK_LIMIT   = L * 200;
    int RESTART_LIMIT    = L * 2000;
    
    long long iter = 0;
    int stuck = 0;
    int total_stuck = 0;
    long long found_count = 0;
    long long total_restarts = 0;

    auto full_restart = [&]() {
        st.randomize(rng); st.full_recalc();
        stuck = 0; total_stuck = 0; tabu.clear();
        total_restarts++;
    };

    full_restart();

    // [CPU Cooling] 強制更頻繁的休眠
    const int SLEEP_BATCH = 2048; 

    while (true) {
        iter++;
        
        if (iter % SLEEP_BATCH == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }

        if (st.violations == 0) {
            if (st.peak_count == 2) { 
                found_count++;
                paths.save(st, true);
                st.mutate(rng, std::max(4, L/3)); stuck = 0; tabu.clear(); continue;
            } 
            else if (st.peak_count <= 4) { 
                if (rng.next_double() < 0.2) paths.save(st, false);
            }
        }

        int best_seq = -1, best_p = -1;
        bool shaping_mode = (st.violations == 0);
        int checks = std::max(10, (int)(L * 0.4));
        int start_k = rng.next_int(L);

        int best_viol_diff = 1000;
        int best_peaks_diff = 1000;
        int best_energy_diff = 1000;

        for(int k=0; k<checks; ++k) {
            int p = (start_k + k) % L;
            int seq_idx = (k % 2); 
            if (shaping_mode && tabu.contains(p)) continue;

            if (!shaping_mode) {
                auto res = st.evaluate_descent(seq_idx, p);
                if (res.d_viol < best_viol_diff) {
                    best_viol_diff = res.d_viol; best_energy_diff = res.d_energy;
                    best_seq = seq_idx; best_p = p;
                    if (res.d_viol < 0) break; 
                } else if (res.d_viol == best_viol_diff) {
                    if (res.d_energy < best_energy_diff) {
                        best_energy_diff = res.d_energy; best_seq = seq_idx; best_p = p;
                    }
                }
            } else {
                auto res = st.evaluate_shaping(seq_idx, p);
                if (!res.valid) continue;
                int current_dist = std::abs(st.peak_count - 2);
                int new_dist = std::abs(st.peak_count + res.d_peaks - 2);
                if (new_dist < current_dist) {
                    best_peaks_diff = -1; best_energy_diff = res.d_energy;
                    best_seq = seq_idx; best_p = p;
                } else if (new_dist == current_dist) {
                    if (res.d_energy < best_energy_diff) {
                        best_peaks_diff = 0; best_energy_diff = res.d_energy;
                        best_seq = seq_idx; best_p = p;
                    }
                }
            }
        }

        bool accept = false;
        if (best_seq != -1) {
            if (!shaping_mode) {
                if (best_viol_diff < 0) accept = true;
                else if (best_viol_diff == 0 && best_energy_diff <= 0) accept = true;
                else if (best_viol_diff == 0 && rng.next_double() < 0.05) accept = true;
            } else {
                if (best_peaks_diff < 0) accept = true;
                else if (best_peaks_diff == 0 && best_energy_diff <= 0) accept = true;
                else if (best_peaks_diff == 0 && rng.next_double() < 0.1) accept = true;
            }
        }

        if (accept) {
            st.apply_flip(best_seq, best_p);
            if (shaping_mode) tabu.add(best_p);
            stuck = 0;
        } else { stuck++; total_stuck++; }

        if (stuck > SMALL_KICK_LIMIT) { st.mutate(rng, 2 + rng.next_int(3)); stuck = 0; tabu.clear(); }
        if (total_stuck > BIG_KICK_LIMIT) { st.mutate(rng, std::max(6, L/4)); total_stuck = 0; tabu.clear(); }
        if (total_stuck > RESTART_LIMIT) full_restart();

        // 降低 I/O 頻率，避免 Race Condition
        if (iter % 50000 == 0) {
            paths.update_dashboard(iter, total_restarts, st.violations, st.peak_count, found_count);
        }
    }
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    if (argc < 4) return 1;
    run_solver(std::stoi(argv[1]), argv[2], std::stoi(argv[3]));
    return 0;
}