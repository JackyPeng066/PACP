#include <iostream>
#include <vector>
#include <cmath>
#include <complex>
#include <algorithm>
#include <map>

using namespace std;

/* =========================
   Global params
   ========================= */
int L;
int TARGET_G;
int ZCZ;
int PREFIX_CANON;
int MAX_SEQ;

const double PI = acos(-1.0);

/* =========================
   Data structure
   ========================= */
struct Seq {
    vector<int> s;
    vector<int> psd;
};

/* =========================
   Canonical (Prefix)
   ========================= */
bool prefix_canonical(const vector<int>& s, int len) {
    for (int sh = 1; sh < len; sh++) {
        for (int i = 0; i < len; i++) {
            if (s[i] < s[(i + sh) % len]) goto next_shift; 
            if (s[i] > s[(i + sh) % len]) return false;    
        }
        return false; 
    next_shift:;
    }
    return true;
}

/* =========================
   Correct DFT (Periodic)
   ========================= */
vector<int> compute_psd(const vector<int>& s) {
    vector<int> psd(L);
    for (int k = 0; k < L; k++) {
        double real_part = 0;
        double imag_part = 0;
        double angle = 2.0 * PI * k / L;
        
        for (int n = 0; n < L; n++) {
            double theta = angle * n;
            real_part += s[n] * cos(theta);
            imag_part -= s[n] * sin(theta);
        }
        // PSD = |DFT|^2
        psd[k] = (int)lround(real_part * real_part + imag_part * imag_part);
    }
    return psd;
}

/* =========================
   DFS Generator
   ========================= */
vector<Seq> pool;

void dfs(int idx, int ones, vector<int>& s) {
    if ((int)pool.size() >= MAX_SEQ) return;

    // Pruning: Hamming weight
    int rem = L - idx;
    if (ones > TARGET_G) return;
    if (ones + rem < TARGET_G) return;

    // Pruning: Prefix Canonical (check every 4 steps to save cost)
    if (idx > 0 && idx % 4 == 0) { 
        if (!prefix_canonical(s, idx)) return;
    }

    if (idx == L) {
        if (!prefix_canonical(s, L)) return; 
        
        Seq q;
        q.s = s;
        q.psd = compute_psd(s);
        pool.push_back(q);
        return;
    }

    s[idx] = 1;
    dfs(idx + 1, ones + 1, s);

    s[idx] = -1;
    dfs(idx + 1, ones, s);
}

/* =========================
   PACF
   ========================= */
int pacf(const vector<int>& s, int u) {
    int r = 0;
    for (int i = 0; i < L; i++)
        r += s[i] * s[(i + u) % L];
    return r;
}

/* =========================
   Main
   ========================= */
int main(int argc, char** argv) {
    if (argc < 6) {
        cerr << "Usage: L g Z prefix max_seq\n";
        return 1;
    }

    L = atoi(argv[1]);
    TARGET_G = atoi(argv[2]);
    ZCZ = atoi(argv[3]);
    PREFIX_CANON = atoi(argv[4]);
    MAX_SEQ = atoi(argv[5]);

    bool is_even = (L % 2 == 0);

    vector<int> s(L);
    dfs(0, 0, s);

    cerr << "Generated " << pool.size() << " sequences. Starting PSD pairing...\n";

    // O(M^2) with PSD filter
    for (size_t i = 0; i < pool.size(); i++) {
        for (size_t j = i; j < pool.size(); j++) { // j starts from i to match symmetric pairs
            
            // --- Layer 1: Frequency Domain (PSD) Filter ---
            bool psd_pass = true;
            for(int k=1; k<L; k++) { 
                int v = pool[i].psd[k] + pool[j].psd[k];
                // Relaxed constraint for Optimal PACP search
                if (abs(v - 2*L) > 10) { 
                    psd_pass = false; 
                    break; 
                }
            }
            if(!psd_pass) continue;

            // --- Layer 2: Time Domain (PACF) Check ---
            bool ok = true;

            for (int u = 1; u < L; u++) {
                int sum_rho = pacf(pool[i].s, u) + pacf(pool[j].s, u);
                
                // ZCZ Check
                if (u < ZCZ && sum_rho != 0) {
                    ok = false;
                    break;
                }

                // Global Constraint Check (Adhikary Targets)
                if (is_even) {
                    // Goal 2: Even Optimal PACP
                    // Condition: 0 everywhere, except |4| at L/2
                    if (u == L / 2) {
                        if (abs(sum_rho) != 4) { ok = false; break; }
                    } else {
                        if (sum_rho != 0) { ok = false; break; }
                    }
                } else {
                    // Goal 1: Odd Optimal PACP
                    // Condition: Magnitude 2 everywhere
                    if (abs(sum_rho) != 2) { ok = false; break; }
                }
            }

            if (ok) {
                cout << "FOUND: ";
                for (int x : pool[i].s) cout << (x == 1 ? '+' : '-');
                cout << " ";
                for (int x : pool[j].s) cout << (x == 1 ? '+' : '-');
                cout << endl;
            }
        }
    }
    return 0;
}