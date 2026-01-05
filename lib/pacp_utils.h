#ifndef PACP_UTILS_H
#define PACP_UTILS_H

#include "pacp_core.h" 
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
#include <iomanip>
#include <cctype>

namespace fs = std::filesystem;

// =========================================================
// 外部符號宣告 (從 pacp_core.o 連結)
// =========================================================

// [Fix] 移除實作，僅保留宣告，避免與 pacp_core.o 重複定義
bool load_result(const std::string& filename, Seq& A, Seq& B);

// =========================================================
// 輔助函式
// =========================================================

inline long long fast_cost(const Seq& A, const Seq& B, int L, int target_val, std::vector<int>& buf_a, std::vector<int>& buf_b) {
    compute_acf(A, buf_a);
    compute_acf(B, buf_b);
    return calc_mse_cost(buf_a, buf_b, L, target_val);
}

inline void chaos_scramble(Seq& s, std::mt19937& rng) {
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

inline void ensure_file_dir(const std::string& filepath) {
    try {
        fs::path p(filepath);
        if (p.has_parent_path()) {
            if (!fs::exists(p.parent_path())) fs::create_directories(p.parent_path());
        }
    } catch (...) {}
}

inline void save_seed_to_file(const std::string& filename, const Seq& A, const Seq& B) {
    ensure_file_dir(filename);
    std::ofstream outfile(filename); 
    if (!outfile.is_open()) return;
    
    for (size_t i = 0; i < A.size(); ++i) outfile << (A[i] == 1 ? '+' : '-');
    outfile << ",";
    for (size_t i = 0; i < B.size(); ++i) outfile << (B[i] == 1 ? '+' : '-');
    outfile << "\n";
    
    outfile.close();
}

inline void append_result_to_file(const std::string& filename, const Seq& A, const Seq& B, int L, int psl) {
    ensure_file_dir(filename);
    std::ofstream outfile(filename, std::ios::app);
    if (!outfile.is_open()) return;

    outfile << L << ",";
    outfile << psl << ",";
    for (size_t i = 0; i < A.size(); ++i) outfile << (A[i] == 1 ? '+' : '-');
    outfile << ",";
    for (size_t i = 0; i < B.size(); ++i) outfile << (B[i] == 1 ? '+' : '-');
    outfile << "\n";
    
    outfile.close();
}

inline void save_session_report(const std::string& base_filename, int L, const std::map<int, int>& stats) {
    std::string report_file = base_filename.substr(0, base_filename.find_last_of('.')) + "_report.log";
    ensure_file_dir(report_file);
    
    std::ofstream outfile(report_file, std::ios::app);
    if (!outfile.is_open()) return;

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
}

inline bool load_seed_csv(const std::string& filename, Seq& A, Seq& B) {
    if (!fs::exists(filename)) return false;
    std::ifstream infile(filename);
    if (!infile.is_open()) return false;

    std::string line;
    if (std::getline(infile, line)) {
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        size_t comma = line.find(',');
        if (comma == std::string::npos) return false;

        std::string sA = line.substr(0, comma);
        std::string sB = line.substr(comma + 1);

        // 基本防呆：若讀到數字(L)開頭，表示格式錯誤 (屬於 result 格式)
        if (!sA.empty() && std::isdigit(sA[0])) return false;

        if (sA.empty() || sB.empty() || sA.length() != sB.length()) return false;
        int L = sA.length();
        A.resize(L); B.resize(L);
        for(int i=0; i<L; ++i) A[i] = (sA[i] == '+') ? 1 : -1;
        for(int i=0; i<L; ++i) B[i] = (sB[i] == '+') ? 1 : -1;
        return true;
    }
    return false;
}

#endif // PACP_UTILS_H