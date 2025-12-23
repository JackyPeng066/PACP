#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <direct.h> // For _mkdir

using namespace std;

// Calc PACF
int calc_pacf(const vector<int>& seq, int u, int L) {
    int sum = 0;
    for (int i = 0; i < L; ++i) sum += seq[i] * seq[(i + u) % L];
    return sum;
}

// DFS Core
void dfs(int idx, int cur_ones, vector<int>& seq, int L, int target_g, int max_rho, ofstream& fout, long long& count) {
    int rem = L - idx;
    if (cur_ones > target_g || cur_ones + rem < target_g) return;

    if (idx == L) {
        if (cur_ones == target_g) {
            bool ok = true;
            for (int u = 1; u <= L / 2; ++u) {
                if (abs(calc_pacf(seq, u, L)) > max_rho) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                for (int x : seq) fout << (x == 1 ? '+' : '-');
                fout << "\n";
                count++;
            }
        }
        return;
    }

    seq[idx] = 1;
    dfs(idx + 1, cur_ones + 1, seq, L, target_g, max_rho, fout, count);

    seq[idx] = -1;
    dfs(idx + 1, cur_ones, seq, L, target_g, max_rho, fout, count);
}

int main(int argc, char* argv[]) {
    if (argc < 5) {
        cout << "Usage: ./aps_dfs <L> <g0> <g1> <max_rho>" << endl;
        return 1;
    }
    int L = atoi(argv[1]);
    int g0 = atoi(argv[2]);
    int g1 = atoi(argv[3]);
    int max_rho = atoi(argv[4]);

    // Create directory path: results/L/g0_g1/
    string dir = "results/" + to_string(L) + "/" + to_string(g0) + "_" + to_string(g1);
    _mkdir("results");
    _mkdir(("results/" + to_string(L)).c_str());
    _mkdir(dir.c_str());

    auto run_search = [&](int g) {
        string path = dir + "/cand_g" + to_string(g) + ".txt";
        ofstream fout(path);
        vector<int> seq(L);
        long long count = 0;
        cout << "Searching g=" << g << " in " << dir << "..." << endl;
        dfs(0, 0, seq, L, g, max_rho, fout, count);
        cout << "Found: " << count << endl;
    };

    run_search(g0);
    if (g0 != g1) run_search(g1);

    return 0;
}