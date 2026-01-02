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
    
    outfile << "Length: " << L << "\n";
    outfile << "Best_PSL: " << psl << "\n";
    outfile << "Total_Unique_Pairs: " << results.size() << "\n";
    outfile << "================================\n";
    
    for(const auto& p : results) {
        for(int x : p.first) outfile << (x==1 ? "+" : "-");
        outfile << "\n";
        for(int x : p.second) outfile << (x==1 ? "+" : "-");
        outfile << "\n";
    }
    outfile.close();
}

bool load_result(const std::string& filename, Seq& a, Seq& b) {
    // 簡單實作讀取 seed 用
    std::ifstream infile(filename);
    if(!infile.is_open()) return false;
    // 略過 header，直接找 + - 
    std::string s;
    std::vector<std::string> seqs;
    while(infile >> s) {
        if(s.find('+') != std::string::npos || s.find('-') != std::string::npos) {
            seqs.push_back(s);
        }
    }
    if(seqs.size() < 2) return false;
    
    auto parse = [](std::string in, Seq& out) {
        out.resize(in.size());
        for(size_t i=0; i<in.size(); ++i) out[i] = (in[i]=='+'||in[i]=='1')?1:-1;
    };
    parse(seqs[0], a);
    parse(seqs[1], b);
    return true;
}