#include "pacp_core.h"
#include <iomanip>
#include <set>

void compute_acf(const Seq& s, std::vector<int>& acf_out) {
    int L = s.size();
    acf_out.assign(L, 0);
    for (int u = 0; u < L; ++u) {
        int sum = 0;
        for (int i = 0; i < L - u; ++i) sum += s[i] * s[i + u];
        acf_out[u] = sum;
    }
}

void compute_periodic_acf(const Seq& S, std::vector<int>& out_acf) {
    int L = S.size();
    if ((int)out_acf.size() != L) out_acf.resize(L);
    for (int k = 0; k < L; ++k) {
        int sum = 0;
        for (int i = 0; i < L; ++i) {
            sum += S[i] * S[(i + k) % L];
        }
        out_acf[k] = sum;
    }
}

void rotate_seq_left(Seq& s, int k) {
    if (s.empty()) return;
    int L = s.size();
    k = (k % L + L) % L;
    std::rotate(s.begin(), s.begin() + k, s.end());
}

int calc_psl(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L) {
    int max_sidelobe = 0;
    for (int u = 1; u < L; ++u) {
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
    auto to_str = [](const Seq& v) {
        std::string out; 
        for(int x : v) out += (x==1 ? "+" : "-");
        return out;
    };

    std::string min_str = to_str(s);
    Seq curr = s;

    // Check all cyclic shifts and negations
    for(int i=0; i<L; ++i) {
        std::string temp = to_str(curr);
        if(temp < min_str) min_str = temp;
        
        Seq neg = curr;
        for(int& x : neg) x *= -1;
        temp = to_str(neg);
        if(temp < min_str) min_str = temp;

        rotate_seq_left(curr, 1);
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
    
    // Header
    outfile << "L=" << L << ",PSL=" << psl << ",Count=" << results.size() << "\n";
    
    // Body (CSV Format: A,B)
    for(const auto& p : results) {
        for(int x : p.first) outfile << (x==1 ? "+" : "-");
        outfile << ",";
        for(int x : p.second) outfile << (x==1 ? "+" : "-");
        outfile << "\n";
    }
    outfile.close();
}

bool load_result(const std::string& filename, Seq& a, Seq& b) {
    std::ifstream infile(filename);
    if(!infile.is_open()) return false;
    
    std::string s;
    std::vector<std::string> seqs;
    
    while(infile >> s) {
        size_t comma_pos = s.find(',');
        if (comma_pos != std::string::npos) {
            std::string part1 = s.substr(0, comma_pos);
            std::string part2 = s.substr(comma_pos + 1);
            if(part1.find_first_of("+-10") != std::string::npos) seqs.push_back(part1);
            if(part2.find_first_of("+-10") != std::string::npos) seqs.push_back(part2);
        } else {
            if(s.length() > 1 && (s.find('+') != std::string::npos || s.find('-') != std::string::npos)) 
                seqs.push_back(s);
        }
    }
    
    if(seqs.size() < 2) return false;
    
    auto parse = [](std::string in, Seq& out) {
        out.clear();
        for(char c : in) {
            if(c == '+' || c == '1') out.push_back(1);
            else if(c == '-' || c == '0') out.push_back(-1);
        }
    };
    
    parse(seqs[seqs.size()-2], a);
    parse(seqs[seqs.size()-1], b);
    return true;
}