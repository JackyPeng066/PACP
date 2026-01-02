#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <set>
#include <cstdlib>
#include <direct.h> // Windows folder creation
#include <io.h>     // Windows file access check

using namespace std;

// 定義權重對
using WeightPair = pair<int, int>;

// 計算平方和：找出 x^2 + y^2 = target，並回推 (g0, g1)
// 用於判斷 PCP (target=L) 或 PACP (target=L+/-2)
vector<WeightPair> get_valid_weights(int target, int L) {
    set<WeightPair> valid_weights;
    for (int x = -L; x <= L; ++x) {
        for (int y = -L; y <= L; ++y) {
            if (x * x + y * y == target) {
                int num_g0 = L + x - y;
                int num_g1 = L - y - x;
                // 必須為偶數才能整除得到整數權重
                if (num_g0 % 2 == 0 && num_g1 % 2 == 0) {
                    int g0 = num_g0 / 2;
                    int g1 = num_g1 / 2;
                    // 權重必須在 [0, L] 範圍內
                    if (g0 >= 0 && g0 <= L && g1 >= 0 && g1 <= L) {
                        if (g0 > g1) swap(g0, g1); // 排序以去重
                        valid_weights.insert({g0, g1});
                    }
                }
            }
        }
    }
    return vector<WeightPair>(valid_weights.begin(), valid_weights.end());
}

// 將權重轉為精簡字串
string format_w(const vector<WeightPair>& weights) {
    if (weights.empty()) return "None";
    string s = "";
    for (size_t i = 0; i < weights.size(); ++i) {
        s += "(" + to_string(weights[i].first) + "," + to_string(weights[i].second) + ")";
        if (i < weights.size() - 1) s += " ";
    }
    return s;
}

int main(int argc, char* argv[]) {
    int start_len = 27;
    int end_len = 100;

    // 參數讀取
    if (argc == 3) {
        start_len = atoi(argv[1]);
        end_len = atoi(argv[2]);
    } else if (argc != 1) {
        cout << "Usage: weight_analysis.exe [start] [end]" << endl;
    }

    string out_dir = "results";
    string out_file = out_dir + "/weight.txt";

    // 建立資料夾
    if (_access(out_dir.c_str(), 0) == -1) {
        _mkdir(out_dir.c_str());
    }

    ofstream f(out_file);
    if (!f.is_open()) return 1;

    // 分類清單 (用於最後總結)
    vector<int> list_pcp, list_pacp, list_pqcp, list_odd;

    cout << "Processing L=" << start_len << " to " << end_len << "..." << endl;
    
    f << "Mode: Priority Filter (PCP > PACP > PQCP)\n";
    f << "Range: " << start_len << " -> " << end_len << "\n";
    f << "--------------------------------------------------------\n";
    f << "L\t| Type\t\t| Weights to Search (g0, g1)\n";
    f << "--------------------------------------------------------\n";

    for (int L = start_len; L <= end_len; ++L) {
        if (L % 2 != 0) {
            // 奇數：直接歸類
            list_odd.push_back(L);
            // 奇數不顯示權重細節，保持版面乾淨
            continue; 
        }

        // 偶數處理邏輯：
        // 1. 先找 PCP
        vector<WeightPair> w_pcp = get_valid_weights(L, L);
        
        if (!w_pcp.empty()) {
            // 情況 A: 存在 PCP。直接輸出，忽略 PACP。
            list_pcp.push_back(L);
            f << L << "\t| [PCP]\t\t| " << format_w(w_pcp) << "\n";
        } else {
            // 情況 B: 不存在 PCP。嘗試找 PACP。
            vector<WeightPair> w_p2 = get_valid_weights(L + 2, L);
            vector<WeightPair> w_m2 = get_valid_weights(L - 2, L);
            
            // 合併 +2 和 -2 的權重
            set<WeightPair> w_pacp_set;
            for(auto w : w_p2) w_pacp_set.insert(w);
            for(auto w : w_m2) w_pacp_set.insert(w);
            
            if (!w_pacp_set.empty()) {
                // 情況 B-1: 存在 Optimal PACP。
                list_pacp.push_back(L);
                vector<WeightPair> final_w(w_pacp_set.begin(), w_pacp_set.end());
                f << L << "\t| [Opt-PACP]\t| " << format_w(final_w) << "\n";
            } else {
                // 情況 B-2: 連 PACP 都不存在 (Adhikary Impossible List)。
                // 這是 PQCP 的目標。
                list_pqcp.push_back(L);
                f << L << "\t| [PQCP Only]\t| (Use Relaxed Constraints: |Sum| <= 4)\n";
            }
        }
    }

    // --- 精簡總結 ---
    f << "\n================ SUMMARY ================\n";
    
    f << "1. PCP Candidates (Priority High): Use weights above\n   [";
    for(size_t i=0; i<list_pcp.size(); ++i) f << list_pcp[i] << (i==list_pcp.size()-1?"":", ");
    f << "]\n\n";

    f << "2. Optimal PACP Targets (PCP Impossible): Use weights above\n   [";
    for(size_t i=0; i<list_pacp.size(); ++i) f << list_pacp[i] << (i==list_pacp.size()-1?"":", ");
    f << "]\n\n";

    f << "3. PQCP Targets (Impossible List): Relax constraints\n   [";
    for(size_t i=0; i<list_pqcp.size(); ++i) f << list_pqcp[i] << (i==list_pqcp.size()-1?"":", ");
    f << "]\n\n";

    f << "4. Odd Candidates (Goal 1): Construct using Legendre/m-seq\n   [";
    for(size_t i=0; i<list_odd.size(); ++i) f << list_odd[i] << (i==list_odd.size()-1?"":", ");
    f << "]\n";

    f.close();
    cout << "Done. Compact report saved to results/weight.txt" << endl;
    return 0;
}