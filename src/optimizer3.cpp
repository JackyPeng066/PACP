/*
   PACP Optimizer v5.1 - C++17 Compatible
   Features:
   1. 128-bit Bitwise Operations (POPCNT)
   2. Template Metaprogramming (Compile-time Constants)
   3. Xoshiro256++ RNG
   4. Late Acceptance Hill Climbing (LAHC)
   5. [Fix] Manual rotl implementation for C++17
*/

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <array>
#include <cstring>
#include <cstdint> // for uint64_t

// 使用 GCC 內建 128-bit 整數
typedef unsigned __int128 uint128;

namespace fs = std::filesystem;

// ---------------------------------------------------------
// 高速亂數產生器 Xoshiro256++
// ---------------------------------------------------------
struct Xoshiro256 {
    uint64_t s[4];

    Xoshiro256(uint64_t seed) {
        // 使用 SplitMix64 初始化狀態
        uint64_t z = (seed += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        s[0] = z ^ (z >> 31);
        z = (seed += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        s[1] = z ^ (z >> 31);
        z = (seed += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        s[2] = z ^ (z >> 31);
        z = (seed += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        s[3] = z ^ (z >> 31);
    }

    // [Fix] 手動實作 rotl 以相容 C++17
    static inline uint64_t rotl(const uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    inline uint64_t next() {
        const uint64_t result = rotl(s[0] + s[3], 23) + s[0];
        const uint64_t t = s[1] << 17;
        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];
        s[2] ^= t;
        s[3] = rotl(s[3], 45);
        return result;
    }
    
    // 產生 0 或 1
    inline int next_bit() {
        return next() & 1;
    }
    
    // 產生 0 ~ range-1
    inline int next_int(int range) {
        return next() % range;
    }
};

// ---------------------------------------------------------
// 核心運算 (Template L 為編譯期常數)
// ---------------------------------------------------------

inline int popcount128(uint128 x) {
    uint64_t low = (uint64_t)x;
    uint64_t high = (uint64_t)(x >> 64);
    return __builtin_popcountll(low) + __builtin_popcountll(high);
}

template <int L>
inline uint128 rotate_left(uint128 x, int k) {
    // 因為 L 是 Template，MASK 會在編譯時直接變成常數，不用運算
    constexpr uint128 MASK = ((uint128)1 << L) - 1;
    
    // 這些運算會被編譯器高度優化
    return ((x << k) & MASK) | (x >> (L - k));
}

// 存檔
void save_result(const std::string& out_dir, uint128 A, uint128 B, int L) {
    if (!fs::exists(out_dir)) fs::create_directories(out_dir);
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::random_device rd;
    std::stringstream ss;
    ss << out_dir << "/L" << L << "_" << now << "_" << std::hex << rd() << ".csv";
    
    std::ofstream outfile(ss.str());
    outfile << L << ",4,";
    // Bit 0 -> '+' (1), Bit 1 -> '-' (-1)
    for(int i=0; i<L; ++i) {
        outfile << (((A >> i) & 1) ? '-' : '+'); 
    }
    outfile << ",";
    for(int i=0; i<L; ++i) {
        outfile << (((B >> i) & 1) ? '-' : '+');
    }
    outfile << "\n";
    outfile.close();
}

template <int L>
bool strict_check(uint128 A, uint128 B) {
    int cnt = 0;
    for (int k = 1; k < L; ++k) {
        int diff_A = popcount128(A ^ rotate_left<L>(A, k));
        int pacf_A = L - 2 * diff_A;
        
        int diff_B = popcount128(B ^ rotate_left<L>(B, k));
        int pacf_B = L - 2 * diff_B;
        
        int sum = pacf_A + pacf_B;
        if (sum != 0) {
            cnt++;
            if (std::abs(sum) != 4) return false;
        }
    }
    return (cnt == 2);
}

// ---------------------------------------------------------
// 演算法核心：針對特定 L 的優化實例
// ---------------------------------------------------------
template <int L>
void run_optimization(const std::string& out_dir) {
    // 隨機初始化
    uint64_t seed_val = std::chrono::system_clock::now().time_since_epoch().count() + std::random_device{}();
    Xoshiro256 rng(seed_val);

    uint128 A = 0, B = 0;
    auto rand_init = [&]() {
        A = 0; B = 0;
        for(int i=0; i<L; ++i) {
            if(rng.next_bit()) A |= ((uint128)1 << i);
            if(rng.next_bit()) B |= ((uint128)1 << i);
        }
    };
    rand_init();

    // 初始能量計算 lambda (Template 版)
    auto calc_energy = [&](uint128 sA, uint128 sB) -> long long {
        long long e = 0;
        for (int k = 1; k < L; ++k) {
            int diff_A = popcount128(sA ^ rotate_left<L>(sA, k));
            int pacf_A = L - 2 * diff_A;
            
            int diff_B = popcount128(sB ^ rotate_left<L>(sB, k));
            int pacf_B = L - 2 * diff_B;
            
            int sum = pacf_A + pacf_B;
            e += (long long)(sum * sum);
            if (e > 32) return 999999; // Early exit
        }
        return e;
    };

    long long curr_e = calc_energy(A, B);
    
    // LAHC Setup
    const int HISTORY_LEN = (L > 60) ? 5000 : 2000;
    std::vector<long long> history(HISTORY_LEN, curr_e);
    int h_idx = 0;
    long long step = 0;
    const long long RESET_STEPS = 5000000LL;

    while (true) {
        step++;

        // 備份
        uint128 old_A = A;
        uint128 old_B = B;

        // 突變
        int p = rng.next_int(L);
        if (rng.next_bit()) A ^= ((uint128)1 << p);
        else                B ^= ((uint128)1 << p);

        // 計算
        long long new_e = calc_energy(A, B);

        // LAHC
        if (new_e <= curr_e || new_e <= history[h_idx]) {
            curr_e = new_e;
            if (curr_e <= 32) {
                if (curr_e == 32 && strict_check<L>(A, B)) {
                    save_result(out_dir, A, B, L);
                    return; // 找到即退出
                }
            }
        } else {
            // 還原
            A = old_A;
            B = old_B;
        }

        // 更新歷史
        history[h_idx] = curr_e;
        h_idx++;
        if (h_idx >= HISTORY_LEN) h_idx = 0;

        // 強制重置
        if (step > RESET_STEPS) {
            step = 0;
            rand_init();
            curr_e = calc_energy(A, B);
            std::fill(history.begin(), history.end(), curr_e);
        }
    }
}

// ---------------------------------------------------------
// Main Dispatcher
// ---------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 3) return 1;
    int target_L = std::stoi(argv[1]);
    std::string out_dir = argv[2];

    switch(target_L) {
        case 44: run_optimization<44>(out_dir); break;
        case 46: run_optimization<46>(out_dir); break;
        case 58: run_optimization<58>(out_dir); break;
        case 68: run_optimization<68>(out_dir); break;
        case 86: run_optimization<86>(out_dir); break;
        case 90: run_optimization<90>(out_dir); break;
        case 94: run_optimization<94>(out_dir); break;
        default: 
            std::cerr << "[Error] L=" << target_L << " not supported in optimized build.\n";
            return 1;
    }

    return 0;
}