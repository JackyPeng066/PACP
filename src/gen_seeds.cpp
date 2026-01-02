#include "../lib/pacp_core.h"
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>

// 判斷質數
bool is_prime(int n) {
    if (n <= 1) return false;
    for (int i = 2; i * i <= n; i++) {
        if (n % i == 0) return false;
    }
    return true;
}

// Legendre Symbol 計算
int legendre_symbol(int a, int p) {
    if (a % p == 0) return 0;
    int res = 1;
    a %= p;
    while (a > 0) {
        while (a % 2 == 0) {
            a /= 2;
            if (p % 8 == 3 || p % 8 == 5) res = -res;
        }
        std::swap(a, p);
        if (a % 4 == 3 && p % 4 == 3) res = -res;
        a %= p;
    }
    if (p == 1) return res;
    return 0;
}

int main(int argc, char* argv[]) {
    std::srand(std::time(0));
    
    // Usage: ./gen_seed <L> <Out> [TargetVal]
    if (argc < 3) return 1;

    int L = std::stoi(argv[1]);
    std::string out_file = argv[2];
    int target_val = (L % 2 != 0) ? 2 : 0;
    if (argc >= 4) target_val = std::stoi(argv[3]);

    Seq seq_a(L), seq_b(L);

    // 策略: 若 Prime 且 L=3 mod 4 -> Legendre; 否則 -> Random
    bool prime = is_prime(L);
    bool use_legendre = prime && (L % 4 == 3);

    if (use_legendre) {
        std::cout << "[Info] L=" << L << " is Prime (3 mod 4). Generating Legendre Sequence." << std::endl;
        for (int i = 0; i < L; ++i) {
            int leg = legendre_symbol(i, L);
            seq_a[i] = (leg == -1) ? -1 : 1;
        }
    } else {
        std::string reason = prime ? "Prime (1 mod 4)" : "Composite";
        std::cout << "[Info] L=" << L << " is " << reason << ". Generating Random Sequence." << std::endl;
        for (int i = 0; i < L; ++i) {
            seq_a[i] = (std::rand() % 2) ? 1 : -1;
        }
    }

    // B 初始設為與 A 相同
    seq_b = seq_a;

    // 計算 Metrics
    std::vector<int> acf_a, acf_b;
    compute_acf(seq_a, acf_a);
    compute_acf(seq_b, acf_b);
    
    // [修正點 1] 使用 calc_mse_cost
    long long init_cost = calc_mse_cost(acf_a, acf_b, L, target_val);
    int init_psl = calc_psl(acf_a, acf_b, L);

    // [修正點 2] 使用 save_result_list
    std::vector<std::pair<Seq, Seq>> seed_list;
    seed_list.push_back({seq_a, seq_b});
    
    save_result_list(out_file, seed_list, L, init_psl);

    std::cout << "[Done] Seed saved to " << out_file 
              << " (MSE: " << init_cost << ", PSL: " << init_psl << ")" << std::endl;

    return 0;
}