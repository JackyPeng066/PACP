/*
   PQCP Accelerator v24.0 - Raptor Lake Edition
   Target: Intel i7-14700 (AVX2, Pipeline Optimization)
   
   Features:
   - Xoshiro256++ RNG (64-bit High Throughput)
   - Branchless Evaluation (Pipeline Stall Elimination)
   - Cache-Friendly Data Layout
*/

#ifndef PQCP_ACCELERATOR_H
#define PQCP_ACCELERATOR_H

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <immintrin.h>

// =========================================================
// [RNG] Xoshiro256++ (State-of-the-art for x64)
// =========================================================
struct Xoshiro256pp {
    uint64_t s[4];

    Xoshiro256pp(uint64_t seed) {
        // SplitMix64 initialization to robustly seed the state
        uint64_t z = (seed + 0x9e3779b97f4a7c15ULL);
        for(int i=0; i<4; ++i) {
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
            z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
            s[i] = z ^ (z >> 31);
        }
    }

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

    // Fast mapping (avoids modulo latency)
    inline int next_int(int range) {
        return (int)((next() * (uint64_t)range) >> 32);
    }
    
    inline double next_double() {
        return (next() >> 11) * 0x1.0p-53;
    }
};

// =========================================================
// [State] Optimized Sequence State
// =========================================================
class SequenceState {
public:
    int L;
    // 使用 std::vector 但在運算時取用 raw pointer
    std::vector<int8_t> A;
    std::vector<int8_t> B;
    std::vector<int> sum_rho;
    
    int violations = 0;   

    SequenceState(int length) : L(length) {
        // Reserve slightly more to avoid boundary checks in vectorized loops if needed
        A.resize(L); B.resize(L);
        sum_rho.resize(L);
    }

    void randomize(Xoshiro256pp& rng) {
        for(int i=0; i<L; ++i) {
            A[i] = (rng.next() & 1) ? 1 : -1;
            B[i] = (rng.next() & 1) ? 1 : -1;
        }
    }

    // O(L^2) Full Recalculation
    void full_recalc() {
        std::fill(sum_rho.begin(), sum_rho.end(), 0);
        
        // Auto-vectorization friendly
        const int8_t* __restrict__ pA = A.data();
        const int8_t* __restrict__ pB = B.data();
        int* __restrict__ pSum = sum_rho.data();

        for (int u = 0; u < L; ++u) {
            int r = 0;
            for (int i = 0; i < L; ++i) {
                // Modulo reduction is expensive, pre-calculated indices or double-buffer is faster
                // But for L < 200, simple logic is okay with -O3
                r += pA[i] * pA[(i + u) % L] + pB[i] * pB[(i + u) % L];
            }
            pSum[u] = r;
        }
        update_metrics();
    }

    void update_metrics() {
        violations = 0;
        int limit = L / 2;
        for (int u = 1; u <= limit; ++u) {
            if (std::abs(sum_rho[u]) > 4) violations++;
        }
    }

    // Strict PQCP Gatekeeper
    bool is_strict_pqcp() const {
        if (violations > 0) return false;
        int peak_cnt = 0;
        for (int u = 1; u < L; ++u) {
            int val = std::abs(sum_rho[u]);
            if (val != 0) {
                if (val != 4) return false; 
                peak_cnt++;
            }
        }
        return (peak_cnt == 2);
    }

    // =========================================================
    // [Core Logic] Branchless Delta Evaluation
    // =========================================================
    struct MoveResult { int d_viol; };

    inline MoveResult evaluate_flip(int seq_idx, int p) const {
        const int8_t* __restrict__ seq = (seq_idx == 0) ? A.data() : B.data();
        const int* __restrict__ srho = sum_rho.data();
        
        int val_p = seq[p];
        int d_viol = 0;
        int limit = L >> 1; // L/2

        // Loop unrolling hint for i7
        #pragma GCC unroll 4
        for (int u = 1; u <= limit; ++u) {
            int p_plus = p + u; if (p_plus >= L) p_plus -= L;
            int p_minus = p - u; if (p_minus < 0) p_minus += L;
            
            int delta = -2 * val_p * (seq[p_plus] + seq[p_minus]);
            if (delta == 0) continue;

            int old_val = srho[u];
            int new_val = old_val + delta;
            
            int old_abs = std::abs(old_val);
            int new_abs = std::abs(new_val);
            
            // --- [Branchless Optimization] ---
            // i7 pipeline hates branches. We use arithmetic logic instead.
            // is_bad = 1 if abs > 4, else 0
            int is_bad_old = (old_abs > 4); 
            int is_bad_new = (new_abs > 4);
            
            // If it becomes bad: +1. If it stops being bad: -1. No change: 0.
            d_viol += (is_bad_new - is_bad_old);
        }
        return {d_viol};
    }

    void apply_flip(int seq_idx, int p) {
        std::vector<int8_t>& seq = (seq_idx == 0) ? A : B;
        int val_p = seq[p];
        int limit = L; 
        
        int* __restrict__ pSum = sum_rho.data();
        const int8_t* __restrict__ pSeq = seq.data();

        #pragma GCC unroll 4
        for (int u = 1; u < limit; ++u) {
             int p_plus = p + u; if (p_plus >= L) p_plus -= L;
             int p_minus = p - u; if (p_minus < 0) p_minus += L;
             int delta = -2 * val_p * (pSeq[p_plus] + pSeq[p_minus]);
             pSum[u] += delta;
        }
        seq[p] = -val_p; 
        update_metrics(); 
    }
    
    void mutate_block(Xoshiro256pp& rng, int size) {
        int start = rng.next_int(L);
        int target = rng.next_int(2);
        for(int k=0; k<size; ++k) {
            apply_flip(target, (start+k)%L);
        }
    }
};

#endif