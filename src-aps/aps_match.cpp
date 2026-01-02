#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <algorithm>

using namespace std;
using namespace std::chrono;

// Periodic Autocorrelation Function (PACF)
int calc_pacf(const string& s, int u, int L) {
    int sum = 0;
    for (int i = 0; i < L; ++i) {
        sum += ((s[i] == '+') ? 1 : -1) * ((s[(i + u) % L] == '+') ? 1 : -1);
    }
    return sum;
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1;

    int L = atoi(argv[1]);
    int g0 = atoi(argv[2]);
    int g1 = atoi(argv[3]);

    // PDF Parity Logic: Odd L (Goal 1) = 2, Even L (Goal 2) = 4
    int target_sum = (L % 2 != 0) ? 2 : 4;

    string dir = "results/" + to_string(L) + "/" + to_string(g0) + "_" + to_string(g1);
    string fileA = dir + "/cand_g" + to_string(g0) + ".txt";
    string fileB = dir + "/cand_g" + to_string(g1) + ".txt";
    string out_path = dir + "/match_result.txt";

    vector<string> poolA, poolB;
    string line;
    ifstream fA(fileA), fB(fileB);
    if (!fA || !fB) return 1;
    
    while (getline(fA, line)) if (!line.empty()) poolA.push_back(line);
    while (getline(fB, line)) if (!line.empty()) poolB.push_back(line);
    fA.close(); fB.close();

    cout << "--- Matching L=" << L << " g=(" << g0 << "," << g1 << ") Target=" << target_sum << " ---" << endl;

    auto start = high_resolution_clock::now();
    ofstream res_out(out_path);
    long long total = (long long)poolA.size() * poolB.size();
    long long checked = 0;
    int found = 0;

    for (const auto& a : poolA) {
        for (const auto& b : poolB) {
            // Symmetry breaking for identical weights
            if (g0 == g1 && a > b) { checked++; continue; }

            bool ok = true;
            for (int u = 1; u <= L / 2; ++u) {
                if (abs(calc_pacf(a, u, L) + calc_pacf(b, u, L)) > target_sum) {
                    ok = false; break;
                }
            }

            if (ok) {
                res_out << a << "," << b << "\n";
                found++;
            }
            if (++checked % 100000 == 0 || checked == total) {
                cout << "\rProgress: " << fixed << setprecision(1) << (double)checked/total*100 << "% | Matches: " << found << flush;
            }
        }
    }
    res_out.close();

    // Sorting for uniqueness
    if (found > 0) {
        string cmd = "sort -u " + out_path + " -o " + out_path;
        system(cmd.c_str());
    }

    auto end = high_resolution_clock::now();
    auto diff = duration_cast<milliseconds>(end - start);
    cout << "\nDone. Found: " << found << " | Time: " << diff.count()/1000.0 << "s" << endl;

    return 0;
}