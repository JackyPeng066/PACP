#include "../lib/pacp_utils.h" 
#include <iostream>

// =========================================================
// 主程式: PACP/PQCP 搜尋引擎 v16.0 (分流版)
// =========================================================

int main(int argc, char* argv[]) {
    // 1. 參數解析與檢查
    if (argc < 6) {
        std::cerr << "Usage: ./optimizer <in_file> <out_file> <fix_a> <target_count> <target_val> [symmetry_mode]\n";
        return 1;
    }

    std::string in_file = argv[1];   
    std::string out_file = argv[2];  // 正選檔案 (例如 data/pacp_L30.csv)
    bool fix_a = (std::stoi(argv[3]) == 1);
    long long target_count_arg = std::stoll(argv[4]); 
    int target_val = std::stoi(argv[5]); 
    int symmetry_mode = 0;
    if (argc >= 7) symmetry_mode = std::stoi(argv[6]);

    // [新增] 自動產生 Candidate 檔名
    // 例如: data/pacp_L30.csv -> data/pacp_L30_candidates.csv
    std::string candidate_file = out_file;
    size_t last_dot = out_file.find_last_of('.');
    if (last_dot != std::string::npos) {
        candidate_file = out_file.substr(0, last_dot) + "_candidates" + out_file.substr(last_dot);
    } else {
        candidate_file += "_candidates";
    }

    ensure_file_dir(in_file);
    ensure_file_dir(out_file);     // 確保正選目錄存在
    ensure_file_dir(candidate_file); // 確保候補目錄存在

    // 2. 載入或生成種子
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
                std::cerr << "[Error] Cannot parse Length (L) from filename or file content.\n";
                return 1; 
            }
        }
    }
    
    int L = A.size();
    if(L < 20) fix_a = false; 

    // 設定存檔門檻
    int save_threshold;
    if (L < 60) {
        save_threshold = 6; 
    } else {
        save_threshold = (target_val > 0) ? (target_val + 4) : 8;
    }

    if (symmetry_mode == 1) {
        for(int i=0; i<L/2; ++i) { A[L-1-i]=A[i]; B[L-1-i]=-B[i]; }
    }

    bool is_hard_mode = (target_val == 0); 

    // 3. 優化參數
    int soft_retry_limit;
    if (L < 40 || is_hard_mode) soft_retry_limit = 2; 
    else soft_retry_limit = 2 + (60 / L);

    long long print_interval;
    if (L < 60) print_interval = 200000; 
    else        print_interval = 50000;

    double alpha_base = (L > 60) ? 10000.0 : 6000.0;
    double alpha = 1.0 - (1.0 / (alpha_base * L)); 
    int max_mutation_k = std::max(1, (int)(L * 0.05));
    double cost_norm_factor = 1.0 / (double)L;
    
    long long inner_max_steps = (L > 60) ? (300000LL * L) : (50000LL * L);
    long long stagnation_limit = inner_max_steps / 4;

    std::cout << "--------------------------------------------------\n";
    std::cout << " HUNTING MODE | L=" << L << " | Target=" << target_val << "\n";
    std::cout << " [Main Output]      : " << out_file << "\n";
    std::cout << " [Candidate Output] : " << candidate_file << " (Threshold: " << save_threshold << ")\n";
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

    std::map<int, int> session_stats;

    long long total_steps = 0;
    int restart_count = 0;
    int prev_global_best_psl = best_psl;
    int global_stagnation_count = 0;
    
    bool force_next_seed = false;

    std::cout << "[Info] Engine Started.\n";

    // 4. 外層迴圈
    while (target_count_arg == 0 || found_count < target_count_arg) {
        
        if (total_steps > 0) {
            if (force_next_seed) {
                // 命中 Target 後，完全隨機重置
                for(int i=0; i<L; ++i) A[i] = (dist_01(rng)<0.5)?1:-1;
                for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
                std::cout << "\n[Hunter] Resetting for next target...\n";
                force_next_seed = false;
                best_psl = 99999; 
                current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
            } 
            else {
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
                    std::cout << "\n[Restart] HARD RESET" << std::endl;
                } 
                else {
                    chaos_scramble(A, rng);
                    for(int i=0; i<L; ++i) B[i] = (dist_01(rng)<0.5)?1:-1;
                    std::cout << "\n[Restart] Chaos Scramble" << std::endl;
                }
                current_cost = fast_cost(A, B, L, target_val, acf_A, acf_B);
            }
        }

        double temp = 5.0;
        int stuck_counter = 0;
        int stuck_threshold = (L > 60) ? (6000 * L) : (3000 * L);
        
        long long inner_step = 0;
        int local_best_psl = 99999;
        long long last_improvement_step = 0;

        // 5. 內層迴圈
        while (inner_step < inner_max_steps) {
            inner_step++;
            total_steps++;

            // Early Pruning
            int protection_threshold = std::max(6, L / 5);
            if (is_hard_mode) protection_threshold += 2; 
            bool is_promising = (local_best_psl <= protection_threshold);

            if (!is_promising && (inner_step - last_improvement_step) > stagnation_limit) {
                std::cout << " -> Stagnation";
                break; 
            }

            // Mutation
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

            // Acceptance
            bool accept = false;
            if (new_cost <= current_cost) accept = true;
            else {
                double delta = (double)(new_cost - current_cost) * cost_norm_factor;
                if (dist_01(rng) < std::exp(-delta / temp)) accept = true;
            }

            if (accept) {
                current_cost = new_cost;
                if (new_cost < old_cost) stuck_counter = 0; else stuck_counter++;

                int psl = calc_psl(acf_A, acf_B, L);
                
                if (psl < local_best_psl) {
                    local_best_psl = psl;
                    last_improvement_step = inner_step;
                }
                if (psl < best_psl) {
                    best_psl = psl;
                }

                // [修正] 分流儲存邏輯
                if (psl <= save_threshold) {
                    std::string ca = get_canonical_repr(A);
                    std::string cb = get_canonical_repr(B);
                    if (ca > cb) std::swap(ca, cb);
                    
                    if (seen_canonical.find({ca, cb}) == seen_canonical.end()) {
                        seen_canonical.insert({ca, cb});
                        session_stats[psl]++;

                        if (psl <= target_val) {
                            // [狀況 A] 命中目標 (Target Hit) -> 存入正選檔案
                            append_result_to_file(out_file, A, B, L, psl);
                            
                            found_count++;
                            std::cout << "\n>>> [TARGET HIT] PSL=" << psl << " Found! (" << found_count << "/" << target_count_arg << ") <<<\n";
                            force_next_seed = true; 
                            break; 
                        } else {
                            // [狀況 B] 僅符合 Candidate 門檻 -> 存入候補檔案 (Silent Save)
                            append_result_to_file(candidate_file, A, B, L, psl);
                            // 不印出訊息，不增加 found_count，繼續搜尋
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
                 std::cout << "\r[Run] Step=" << inner_step << " | GlobalBest=" << best_psl << " | Local=" << local_best_psl << " | Hits=" << found_count << "   " << std::flush;
            }
        } 
    }
    
    std::cout << "\n[Done] Process Completed. Unique Solutions: " << session_stats.size() << std::endl;
    save_session_report(out_file, L, session_stats);
    return 0;
}