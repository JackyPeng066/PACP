#include "../lib/pacp_core.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <set>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <map>
#include <iomanip> // 用於時間格式化

namespace fs = std::filesystem;

// =========================================================
// 輔助函式
// =========================================================

long long fast_cost(const Seq& A, const Seq& B, int L, int target_val, std::vector<int>& buf_a, std::vector<int>& buf_b) {
    compute_acf(A, buf_a);
    compute_acf(B, buf_b);
    return calc_mse_cost(buf_a, buf_b, L, target_val);
}

void chaos_scramble(Seq& s, std::mt19937& rng) {
    int L = s.size();
    if (L == 0) return;
    int shift1 = (L * 13) / 32; if (shift1 == 0) shift1 = 1;
    rotate_seq_left(s, shift1);
    std::uniform_int_distribution<int> dist_idx(0, L - 1);
    int p1 = dist_idx(rng);
    int p2 = dist_idx(rng);
    if (p1 > p2) std::swap(p1, p2);
    for (int i = p1; i <= p2; ++i) s[i] *= -1;
    int shift2 = (L * 7) / 32; if (shift2 == 0) shift2 = 1;
    rotate_seq_left(s, L - shift2);
    int shift3 = (L * 5) / 32; if (shift3 == 0) shift3 = 1;
    rotate_seq_left(s, shift3);
}

void ensure_file_dir(const std::string& filepath) {
    try {
        fs::path p(filepath);
        if (p.has_parent_path()) {
            if (!fs::exists(p.parent_path())) fs::create_directories(p.parent_path());
        }
    } catch (...) {}
}

// [寫入] 種子存檔格式：A,B (維持簡單，因為這只是起點)
void save_seed_to_file(const std::string& filename, const Seq& A, const Seq& B) {
    ensure_file_dir(filename);
    std::ofstream outfile(filename); 
    if (!outfile.is_open()) return;
    
    for (size_t i = 0; i < A.size(); ++i) outfile << (A[i] == 1 ? '+' : '-');
    outfile << ",";
    for (size_t i = 0; i < B.size(); ++i) outfile << (B[i] == 1 ? '+' : '-');
    outfile << "\n";
    
    outfile.close();
}

// [升級] 結果追加格式：L,PSL,A,B
// 這樣未來找 SZCP 時，可以直接讀取前兩欄進行篩選，不用重算
void append_result_to_file(const std::string& filename, const Seq& A, const Seq& B, int L, int psl) {
    ensure_file_dir(filename);
    std::ofstream outfile(filename, std::ios::app);
    if (!outfile.is_open()) return;

    // Col 1: L
    outfile << L << ",";
    // Col 2: PSL
    outfile << psl << ",";
    // Col 3: A
    for (size_t i = 0; i < A.size(); ++i) outfile << (A[i] == 1 ? '+' : '-');
    outfile << ",";
    // Col 4: B
    for (size_t i = 0; i < B.size(); ++i) outfile << (B[i] == 1 ? '+' : '-');
    outfile << "\n";
    
    outfile.close();
}

// [新增] 儲存本次執行報告 (給人看的)
void save_session_report(const std::string& base_filename, int L, const std::map<int, int>& stats) {
    // 檔名範例: pacp_L30_report.log
    std::string report_file = base_filename.substr(0, base_filename.find_last_of('.')) + "_report.log";
    ensure_file_dir(report_file);
    
    std::ofstream outfile(report_file, std::ios::app);
    if (!outfile.is_open()) return;

    // 取得當前時間
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    outfile << "==================================================\n";
    outfile << "[Session] " << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << "\n";
    outfile << "Length (L) : " << L << "\n";
    
    if (stats.empty()) {
        outfile << "Result     : No sequences found in this run.\n";
    } else {
        outfile << "Found Items:\n";
        int total = 0;
        for (const auto& kv : stats) {
            outfile << "  - PSL = " << kv.first << " : " << kv.second << " counts\n";
            total += kv.second;
        }
        outfile << "Total Found: " << total << "\n";
    }
    outfile << "==================================================\n\n";
    outfile.close();
    
    std::cout << "[Report] Session stats saved to " << report_file << "\n";
}

// [讀取] 專門讀取 A,B 格式的 CSV 種子檔
bool load_seed_csv(const std::string& filename, Seq& A, Seq& B) {
    if (!fs::exists(filename)) return false;
    std::ifstream infile(filename);
    if (!infile.is_open()) return false;

    std::string line;
    if (std::getline(infile, line)) {
        line.erase(remove_if(line.begin(), line.end(), isspace), line.end());
        size_t comma = line.find(',');
        if (comma == std::string::npos) return false;

        std::string sA = line.substr(0, comma);
        std::string sB = line.substr(comma + 1);

        if (sA.empty() || sB.empty() || sA.length() != sB.length()) return false;
        int L = sA.length();
        A.resize(L); B.resize(L);
        for(int i=0; i<L; ++i) A[i] = (sA[i] == '+') ? 1 : -1;
        for(int i=0; i<L; ++i) B[i] = (sB[i] == '+') ? 1 : -1;
        return true;
    }
    return false;
}

// =========================================================
// 主程式
// =========================================================

int main(int argc, char* argv[]) {
    if (argc < 4) return 1; 

    std::string in_file = argv[1];   
    std::string out_file = argv[2];  
    bool fix_a = (std::stoi(argv[3]) == 1);
    long long target_count_arg = std::stoll(argv[4]); 
    int target_val = std::stoi(argv[5]);
    int symmetry_mode = 0;
    if (argc >= 7) symmetry_mode = std::stoi(argv[6]);

    ensure_file_dir(in_file);

    Seq A, B;
    if (!load_seed_csv(in_file, A, B)) {
        if (!load_result(in_file, A, B)) {
            int parsed_L = 0;
            size_t pos = in_file.find("_L");
            if (pos != std::string::npos) {
                try {
                    std::string num_part = in_file.substr(pos + 2);
                    size_t end_pos = num_part.find_first_not_of("0123456789");
                    if (end_pos != std::string::npos) num_part = num_part.substr(0, end_pos);
                    parsed_L = std::stoi(num_part);
                } catch(...) {}
            }
            
            if (parsed_L > 0) {
                std::cout << "[Info] Generating NEW random seed for L=" << parsed_L << "\n";
                A.resize(parsed_L); B.resize(parsed_L);
                unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                std::mt19937 rng_init(seed);
                std::uniform_real_distribution<double> dist(0.0, 1.0);
                for(int i=0; i<parsed_L; ++i) A[i] = (dist(rng_init)<0.5)?1:-1;
                for(int i=0; i<parsed_L; ++i) B[i] = (dist(rng_init)<0.5)?1:-1;
            } else {
                return 1; 
            }
        }
    }
    
    int L = A.size();
    if(L < 20) fix_a = false; 

    if (symmetry_mode == 1) {
        for(int i=0; i<L/2; ++i) { A[L-1-i]=A[i]; B[L-1-i]=-B[i]; }
    }

    bool is_hard_mode = (target_val == 0);

    // Aggressive Reset Strategy
    int soft_retry_limit;
    if (L < 40 || is_hard_mode) soft_retry_limit = 2; 
    else soft_retry_limit = 2 + (60 / L);

    long long print_interval;
    if (L < 60) print_interval = 50000;
    else        print_interval = 5000;

    double alpha_base = (L > 60) ? 10000.0 : 6000.0;
    double alpha = 1.0 - (1.0 / (alpha_base * L)); 
    int max_mutation_k = std::max(1, (int)(L * 0.05));
    double cost_norm_factor = 1.0 / (double)L;
    
    long long inner_max_steps = (L > 60) ? (300000LL * L) : (50000LL * L);
    long long stagnation_limit = inner_max_steps / 4;

    std::cout << "--------------------------------------------------\n";
    std::cout << " OPTIMIZER v14.0 (Metadata) | L=" << L << " | Target=" << target_val << "\n";
    std::cout << " Output Format: L,PSL,A,B (Enhanced CSV)\n";
    std::cout << "--------------------------------------------------\n";

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist_01(0.0, 1.0);
    std::uniform_int_distribution<int> dist_idx(0, L - 1);
    std::uniform_int_distribution<int> dist_rot(1, L - 1);

    std::vector<int> acf_A(L), acf_B(L);
    long long current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
    int best_psl = 99999; 

    std::vector<std::pair<Seq, Seq>> results_buffer; 
    std::set<std::pair<std::string, std::string>> seen_canonical;
    long long found_count = 0;

    // [新增] 本次執行的統計數據 (PSL -> Count)
    std::map<int, int> session_stats;

    long long total_steps = 0;
    int restart_count = 0;
    int prev_global_best_psl = best_psl;
    int global_stagnation_count = 0;

    std::cout << "[Info] Engine Started.\n";

    while (target_count_arg == 0 || found_count < target_count_arg) {
        
        // --- Restart Decision ---
        if (total_steps > 0) {
            restart_count++;
            
            if (best_psl == prev_global_best_psl) global_stagnation_count++;
            else { global_stagnation_count = 0; prev_global_best_psl = best_psl; }

            bool do_hard_reset = false;
            if (restart_count % soft_retry_limit == 0) do_hard_reset = true;
            if (global_stagnation_count >= 2) do_hard_reset = true;

            if (fix_a) {
                int r = dist_rot(rng);
                rotate_seq_left(A, r);
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
            } 
            else if (do_hard_reset) {
                for(int i=0; i<L; ++i) A[i] = (dist_01(rng)<0.5)?1:-1;
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
                save_seed_to_file(in_file, A, B);
                std::cout << "\n[Restart] HARD RESET (New Seed Saved)" << std::endl;
            } 
            else {
                chaos_scramble(A, rng);
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
                std::cout << "\n[Restart] Chaos Scramble" << std::endl;
            }
            current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
        }

        double temp = 5.0;
        int stuck_counter = 0;
        int stuck_threshold = (L > 60) ? (6000 * L) : (3000 * L);
        
        long long inner_step = 0;
        int local_best_psl = 99999;
        long long last_improvement_step = 0;

        while (inner_step < inner_max_steps) {
            inner_step++;
            total_steps++;

            int protection_threshold = std::max(6, L / 5);
            if (is_hard_mode) protection_threshold += 2; 
            bool is_promising = (local_best_psl <= protection_threshold);

            if (!is_promising && (inner_step - last_improvement_step) > stagnation_limit) {
                std::cout << " -> Stagnation (Stuck " << stagnation_limit << " steps)";
                break; 
            }

            // 10% Flash Check
            if (inner_step == (long long)(inner_max_steps * 0.1)) {
                int flash_limit = (int)(L * 0.6);
                int check_psl = calc_psl(acf_A, acf_B, L);
                
                if (check_psl < local_best_psl) {
                    local_best_psl = check_psl;
                    last_improvement_step = inner_step;
                }
                
                if (check_psl <= best_psl) {
                    std::string ca = get_canonical_repr(A);
                    std::string cb = get_canonical_repr(B);
                    if (ca > cb) std::swap(ca, cb);
                    
                    if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                        seen_canonical.insert({ca, cb});
                        
                        // [升級] 存入 L,PSL,A,B
                        append_result_to_file(out_file, A, B, L, check_psl);
                        session_stats[check_psl]++; // 紀錄統計
                        
                        std::cout << "\n[Found Result] PSL=" << check_psl << " (Saved) @ Flash Check" << std::flush;
                        
                        if (check_psl < best_psl) {
                            best_psl = check_psl;
                            prev_global_best_psl = best_psl; 
                            found_count = 0; 
                        } else {
                            found_count++; 
                        }
                    }
                }

                if (local_best_psl > flash_limit) {
                    std::cout << " -> Flash Exit (Bad Seed PSL=" << local_best_psl << ")";
                    break; 
                }
            }

            if (inner_step == (long long)(inner_max_steps * 0.3)) {
                int limit = (L > 80) ? (L / 2) : (L / 3);
                if (is_hard_mode) limit += 4;
                if (local_best_psl > std::max(10, limit)) { std::cout << " -> Exit 30%"; break; }
            }
            if (inner_step == (long long)(inner_max_steps * 0.6)) {
                int limit = (L > 80) ? (L / 3) : (L / 4 + 4);
                if (is_hard_mode) limit += 6;
                if (local_best_psl > std::max(12, limit)) { std::cout << " -> Exit 60%"; break; }
            }

            Seq old_A = A; Seq old_B = B; long long old_cost = current_cost;

            bool is_rotate_move = (dist_01(rng) < 0.05);
            if (is_rotate_move) {
                int rot_k = dist_rot(rng);
                if (fix_a) rotate_seq_left(A, rot_k);
                else {
                    if (dist_01(rng) < 0.5) rotate_seq_left(A, rot_k);
                    else rotate_seq_left(B, rot_k);
                }
            } else {
                int current_k = 1;
                if (temp > 1.0 && max_mutation_k > 1) {
                    std::uniform_int_distribution<int> dist_k(1, max_mutation_k);
                    current_k = dist_k(rng);
                }
                for(int k=0; k<current_k; ++k) {
                    bool flip_a = fix_a ? false : (dist_01(rng) < 0.5);
                    if (flip_a) A[dist_idx(rng)] *= -1;
                    else        B[dist_idx(rng)] *= -1;
                }
            }

            long long new_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);

            bool accept = false;
            if (new_cost <= current_cost) accept = true;
            else {
                double delta = (double)(new_cost - current_cost) * cost_norm_factor;
                if (dist_01(rng) < std::exp(-delta / temp)) accept = true;
            }

            if (accept) {
                current_cost = new_cost;
                if (new_cost < old_cost) stuck_counter = 0; else stuck_counter++;

                if (current_cost <= L) {
                    int psl = calc_psl(acf_A, acf_B, L);
                    
                    if (psl < local_best_psl) {
                        local_best_psl = psl;
                        last_improvement_step = inner_step;
                    }

                    if (psl <= best_psl) {
                        std::string ca = get_canonical_repr(A);
                        std::string cb = get_canonical_repr(B);
                        if (ca > cb) std::swap(ca, cb);
                        
                        if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                            seen_canonical.insert({ca, cb});
                            
                            // [升級] 存入 L,PSL,A,B
                            append_result_to_file(out_file, A, B, L, psl);
                            session_stats[psl]++; // 紀錄統計
                            
                            std::cout << "\n[Found Result] PSL=" << psl << " (Saved) @ Total=" << total_steps << "   " << std::flush;
                            
                            if (psl < best_psl) {
                                best_psl = psl;
                                prev_global_best_psl = best_psl; 
                                global_stagnation_count = 0;
                                found_count = 0; 
                            } else {
                                found_count++;
                            }

                            bool goal_reached = false;
                            if (best_psl == target_val) goal_reached = true;
                            if (target_val == 0 && best_psl <= 2) { goal_reached = true; }

                            if (goal_reached && (target_count_arg > 0 && found_count >= target_count_arg)) goto FINISH;
                        }
                    }
                }
            } else {
                A = old_A; B = old_B; current_cost = old_cost; stuck_counter++;
            }

            temp *= alpha;

            if (stuck_counter > stuck_threshold || temp < 0.001) {
                temp = 5.0; stuck_counter = 0;
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
                current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
            }
            
            if (inner_step % print_interval == 0) {
                 std::cout << "\r[Running] Step=" << inner_step << " | Best=" << best_psl << " | Loc=" << local_best_psl << "   " << std::flush;
            }
        } 
    }
FINISH:
    std::cout << "\n[Done] Best PSL Found: " << best_psl << " | Unique Count: " << results_buffer.size() << std::endl;
    // [新增] 程式結束時，輸出本次執行的統計報告
    save_session_report(out_file, L, session_stats);
    return 0;
}