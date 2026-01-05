#ifndef PACP_IO_H
#define PACP_IO_H

#include "pacp_core.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <iomanip>
#include <algorithm>
#include <set>
#include <sstream> // 補上

namespace fs = std::filesystem;

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
    outfile << L << "," << psl << ",";
    for (size_t i = 0; i < A.size(); ++i) outfile << (A[i] == 1 ? '+' : '-');
    outfile << ",";
    for (size_t i = 0; i < B.size(); ++i) outfile << (B[i] == 1 ? '+' : '-');
    outfile << "\n";
    outfile.close();
}

inline void append_szcp_to_file(const std::string& base_filename, const Seq& A, const Seq& B, int L, int psl, int zcz) {
    std::string szcp_file = base_filename.substr(0, base_filename.find_last_of('.')) + "_SZCP.txt";
    ensure_file_dir(szcp_file);
    std::ofstream outfile(szcp_file, std::ios::app);
    if (!outfile.is_open()) return;
    outfile << L << "," << psl << "," << zcz << ","; 
    for (int x : A) outfile << (x == 1 ? '+' : '-');
    outfile << ",";
    for (int x : B) outfile << (x == 1 ? '+' : '-');
    outfile << "\n";
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
    outfile << "L=" << L << "\n";
    if (stats.empty()) outfile << "Result: None.\n";
    else {
        outfile << "Found:\n";
        for (const auto& kv : stats) outfile << "  PSL=" << kv.first << ": " << kv.second << "\n";
    }
    outfile << "==================================================\n\n";
    outfile.close();
    std::cout << "[Report] Session stats saved to " << report_file << "\n";
}

inline bool parse_csv_line(const std::string& line, int& L, int& psl, Seq& A, Seq& B) {
    std::stringstream ss(line);
    std::string segment;
    std::vector<std::string> parts;
    while(std::getline(ss, segment, ',')) parts.push_back(segment);
    if (parts.size() < 4) return false;
    try {
        L = std::stoi(parts[0]);
        psl = std::stoi(parts[1]);
        std::string sA = parts[2];
        std::string sB = parts[3];
        sA.erase(std::remove_if(sA.begin(), sA.end(), ::isspace), sA.end());
        sB.erase(std::remove_if(sB.begin(), sB.end(), ::isspace), sB.end());
        A.resize(L); B.resize(L);
        for(int i=0; i<L; ++i) A[i] = (sA[i] == '+') ? 1 : -1;
        for(int i=0; i<L; ++i) B[i] = (sB[i] == '+') ? 1 : -1;
        return true;
    } catch (...) { return false; }
}

inline bool load_seed_csv(const std::string& filename, Seq& A, Seq& B) {
    if (!fs::exists(filename)) return false;
    std::ifstream infile(filename);
    std::string line;
    if (std::getline(infile, line)) {
        int L, psl_dummy; 
        if (line.find(',') != std::string::npos) {
             return parse_csv_line(line, L, psl_dummy, A, B) || 
                    parse_csv_line("0,0,"+line, L, psl_dummy, A, B); 
        }
    }
    return false;
}

inline void load_existing_results(const std::string& filename, std::set<std::pair<std::string, std::string>>& seen) {
    if (!fs::exists(filename)) return;
    std::ifstream infile(filename);
    std::string line;
    while (std::getline(infile, line)) {
        if (line.empty() || line[0] == '#') continue;
        int L, psl; Seq A, B;
        if (parse_csv_line(line, L, psl, A, B)) {
            std::string ca = get_canonical_repr(A);
            std::string cb = get_canonical_repr(B);
            if (ca > cb) std::swap(ca, cb);
            seen.insert({ca, cb});
        }
    }
}

#endif