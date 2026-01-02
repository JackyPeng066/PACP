#include "pacp_core.h"
#include <iomanip>
#include <set>

void compute_acf(const Seq& s, std::vector<int>& acf_out) {
    int L = s.size();
    acf_out.assign(L, 0);
    for (int u = 0; u < L; ++u) {
        int sum = 0;
        for (int i = 0; i < L; ++i) sum += s[i] * s[(i + u) % L];
        acf_out[u] = sum;
    }
}

void rotate_seq_left(Seq& s, int k) {
    if (s.empty()) return;
    int L = s.size();
    k = k % L; 
    if (k < 0) k += L;
    
    // 使用 std::rotate (需 include <algorithm>)
    // 將前 k 個元素移到後面
    std::rotate(s.begin(), s.begin() + k, s.end());
}

int calc_psl(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L) {
    int max_sidelobe = 0;
    for (int u = 1; u < L; ++u) { // 不看 u=0
        int val = std::abs(acf_a[u] + acf_b[u]);
        if (val > max_sidelobe) max_sidelobe = val;
    }
    return max_sidelobe;
}

long long calc_mse_cost(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L, int target_val) {
    long long cost = 0;
    for (int u = 1; u < L; ++u) {
        int diff = std::abs(acf_a[u] + acf_b[u]) - target_val;
        cost += (long long)(diff * diff);
    }
    return cost;
}

std::string get_canonical_repr(const Seq& s) {
    int L = s.size();
    std::string min_str = "";
    
    // 轉字串 helper
    auto to_str = [](const Seq& v) {
        std::string out; 
        for(int x : v) out += (x==1 ? "+" : "-");
        return out;
    };

    min_str = to_str(s);
    Seq curr = s;

    // 遍歷所有移位
    for(int i=0; i<L; ++i) {
        // 正相
        std::string temp = to_str(curr);
        if(temp < min_str) min_str = temp;
        
        // 反相
        Seq neg = curr;
        for(int& x : neg) x *= -1;
        temp = to_str(neg);
        if(temp < min_str) min_str = temp;

        // Rotate
        int first = curr[0];
        for(int k=0; k<L-1; ++k) curr[k] = curr[k+1];
        curr[L-1] = first;
    }
    return min_str;
}

void int_to_seq(int val, int L, Seq& s) {
    s.resize(L);
    for (int i = 0; i < L; ++i) {
        s[i] = ((val >> i) & 1) ? -1 : 1;
    }
}


void save_result_list(const std::string& filename, const std::vector<std::pair<Seq, Seq>>& results, int L, int psl) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) return;
    
    outfile << "L=" << L << ",PSL=" << psl << ",Count=" << results.size() << "\n";
    
    for(const auto& p : results) {
        // 輸出 A
        for(int x : p.first) outfile << (x==1 ? "+" : "-");
        
        outfile << ","; // 逗號分隔
        
        // 輸出 B
        for(int x : p.second) outfile << (x==1 ? "+" : "-");
        
        outfile << "\n"; // 換行
    }
    outfile.close();
}

bool load_result(const std::string& filename, Seq& a, Seq& b) {
    std::ifstream infile(filename);
    if(!infile.is_open()) return false;
    
    std::string s;
    std::vector<std::string> seqs;
    
    // 讀取檔案中所有包含 + 或 - 的 token
    while(infile >> s) {
        // 檢查是否包含分隔符號 ',' (適應新格式 A,B)
        size_t comma_pos = s.find(',');
        if (comma_pos != std::string::npos) {
            // 找到逗號，拆分成兩段
            std::string part1 = s.substr(0, comma_pos);
            std::string part2 = s.substr(comma_pos + 1);
            
            // 簡單過濾，確保有內容
            if(part1.find_first_of("+-10") != std::string::npos) seqs.push_back(part1);
            if(part2.find_first_of("+-10") != std::string::npos) seqs.push_back(part2);
        } 
        else {
            // 舊格式 (或只有一段)，直接判斷
            if(s.find('+') != std::string::npos || s.find('-') != std::string::npos) {
                if (s.length() > 1) seqs.push_back(s);
            }
        }
    }
    
    if(seqs.size() < 2) return false;
    
    auto parse = [](std::string in, Seq& out) {
        std::vector<int> temp;
        for(char c : in) {
            if(c == '+' || c == '1') temp.push_back(1);
            else if(c == '-' || c == '0') temp.push_back(-1);
            // 忽略其他字元 (如逗號、空格)
        }
        out = temp;
    };
    
    // 取最後兩組序列作為種子
    parse(seqs[seqs.size()-2], a);
    parse(seqs[seqs.size()-1], b);
    
    return true;
}