#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <fstream>
#include <direct.h>

using namespace std;

bool is_golay(int L) {
    if (L <= 0) return false;
    int t = L;
    while (t % 2 == 0) t /= 2;
    while (t % 5 == 0) t /= 5; 
    while (t % 13 == 0) t /= 13; 
    return (t == 1);
}

void mkdir_p(const string& d) { _mkdir(d.c_str()); }

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: aps_weight <length>\n";
        return 1;
    }

    int L = atoi(argv[1]);
    if (L <= 0) return 1;

    if (is_golay(L)) {
        cout << "L=" << L << " is Golay. Skip recommended.\n";
    }

    string b_dir = "results";
    string l_dir = b_dir + "/" + to_string(L);
    mkdir_p(b_dir);
    mkdir_p(l_dir);

    ofstream fout(l_dir + "/" + to_string(L) + "_weight.txt");
    int count = 0;

    if (L % 2 == 0) {
        // Theorem 2: L +/- 2 = (g0-g1)^2 + (L-g0-g1)^2
        for (int g0 = 0; g0 <= L; ++g0) {
            for (int g1 = 0; g1 <= L; ++g1) {
                // Symmetry Pruning: skip equivalent pairs (e.g., 4 7 vs 7 4)
                if (g0 > g1) continue; 

                long long s = (long long)pow(g0 - g1, 2) + (long long)pow(L - g0 - g1, 2);
                if (s == (L + 2) || s == (L - 2)) {
                    fout << g0 << " " << g1 << "\n";
                    count++;
                }
            }
        }
    } else {
        // Goal 1: Odd L balancing heuristics
        for (int g0 = L/2; g0 <= L/2 + 1; ++g0) {
            for (int g1 = L/2; g1 <= L/2 + 1; ++g1) {
                // Symmetry Pruning
                if (g0 > g1) continue; 

                fout << g0 << " " << g1 << "\n";
                count++;
            }
        }
    }
    fout.close();
    cout << "Success: Found " << count << " unique weight pairs.\n";
    return 0;
}