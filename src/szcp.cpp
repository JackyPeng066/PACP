#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;
using namespace std;

// 計算週期自相關函數 (Periodic ACF)
int calc_pacf(const string& s, int u, int L) {
    int sum = 0;
    for (int i = 0; i < L; ++i) {
        int val_i = (s[i] == '+') ? 1 : -1;
        int val_iu = (s[(i + u) % L] == '+') ? 1 : -1;
        sum += val_i * val_iu;
    }
    return sum;
}

// 驗證是否符合 Optimal (L, L/2)-SZCP 定義
// 條件 1: ZCZ 寬度 Z = L/2 (u=1 到 L/2-1 之和必須為 0)
// 條件 2: Out-of-zone magnitude 等於 2 (u=L/2 之和絕對值必須為 2)
bool is_optimal_szcp(const string& sA, const string& sB, int L) {
    int Z = L / 2;
    // 條件 1: 區域內 ZCZ 檢測
    for (int u = 1; u < Z; ++u) {
        if ((calc_pacf(sA, u, L) + calc_pacf(sB, u, L)) != 0) return false;
    }
    // 條件 2: 區域外 Magnitude 檢測
    int out_zone_val = abs(calc_pacf(sA, Z, L) + calc_pacf(sB, Z, L));
    return (out_zone_val == 2);
}

int main() {
    string src_base = "results-final/Goal2";
    string dst_base = "results-final/Goal2-1";
    string report_path = "results-final/optimal_szcp_summary.txt";

    if (!fs::exists(src_base)) {
        cerr << "錯誤: 找不到來源路徑 " << src_base << endl;
        return 1;
    }

    ofstream report(report_path);
    auto now = chrono::system_clock::to_time_t(chrono::system_clock::now());
    report << "Optimal SZCP Verification Report - " << put_time(localtime(&now), "%Y-%m-%d %H:%M:%S") << endl;
    report << "==========================================================" << endl;
    report << left << setw(10) << "Length L" << " | " << setw(15) << "Found SZCP" << endl;
    report << "----------------------------------------------------------" << endl;

    cout << ">>> 開始自動掃描並驗證 Optimal SZCP..." << endl;

    int total_l_processed = 0;
    vector<int> found_l_list;

    // 1. 自動遍歷 Goal2 下的所有資料夾
    for (const auto& entry : fs::directory_iterator(src_base)) {
        if (!entry.is_directory()) continue;

        string l_str = entry.path().filename().string();
        int L = stoi(l_str);
        string src_file = entry.path().string() + "/pacp_L" + l_str + ".txt";

        if (!fs::exists(src_file)) continue;

        string dst_dir = dst_base + "/" + l_str;
        string dst_file = dst_dir + "/pacp_L" + l_str + ".txt";

        ifstream infile(src_file);
        int found_count = 0;
        string line;
        stringstream ss_out;

        while (getline(infile, line)) {
            if (line.empty()) continue;
            stringstream ss(line);
            string part;
            vector<string> tokens;
            while (getline(ss, part, ',')) tokens.push_back(part);

            if (tokens.size() < 4) continue;
            if (is_optimal_szcp(tokens[2], tokens[3], L)) {
                ss_out << line << "\n";
                found_count++;
            }
        }

        // 2. 如果有找到，建立資料夾並寫入結果
        if (found_count > 0) {
            fs::create_directories(dst_dir);
            ofstream outfile(dst_file);
            outfile << ss_out.str();
            found_l_list.push_back(L);
        }

        report << left << setw(10) << L << " | " << setw(15) << found_count << endl;
        cout << "處理 L=" << left << setw(4) << L << " | 找到: " << found_count << endl;
        total_l_processed++;
    }

    // 3. 輸出總結摘要
    report << "----------------------------------------------------------" << endl;
    report << "Total Lengths Scanned: " << total_l_processed << endl;
    report << "szcp_found_at: ";
    for (size_t i = 0; i < found_l_list.size(); ++i) {
        report << found_l_list[i] << (i == found_l_list.size() - 1 ? "" : ", ");
    }
    report << "\n==========================================================" << endl;

    cout << ">>> 驗證完成！統計報告已生成: " << report_path << endl;
    return 0;
}