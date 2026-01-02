#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <set>
#include <algorithm>
#include <chrono>
#include <iomanip>

using namespace std;
using namespace std::chrono;

// Generate the canonical (lexicographical smallest) form of a sequence
// Handles: Cyclic Shift, Reversal, and Negation
string get_canonical(string s) {
    int L = s.length();
    string canonical = s;
    for (int n = 0; n < 2; ++n) { // Negation
        for (int r = 0; r < 2; ++r) { // Reversal
            string temp = s;
            for (int k = 0; k < L; ++k) { // All cyclic shifts
                if (temp < canonical) canonical = temp;
                rotate(temp.begin(), temp.begin() + 1, temp.end());
            }
            reverse(s.begin(), s.end());
        }
        for (char &c : s) c = (c == '+') ? '-' : '+';
    }
    return canonical;
}

int main(int argc, char* argv[]) {
    // Standard input pattern: L g0 g1
    if (argc < 4) {
        cout << "Usage: ./aps_filter <L> <g0> <g1>" << endl;
        return 1;
    }

    int L = atoi(argv[1]);
    int g0 = atoi(argv[2]);
    int g1 = atoi(argv[3]);

    string dir = "results/" + to_string(L) + "/" + to_string(g0) + "_" + to_string(g1);
    string in_path = dir + "/match_result.txt";
    string out_path = dir + "/unique_result.txt";

    ifstream fin(in_path);
    if (!fin) {
        cerr << "Error: Cannot find match_result.txt in " << dir << endl;
        return 1;
    }

    cout << "-------------------------------------------" << endl;
    cout << " APS Filter | Stage 4 | L=" << L << " | Weight: " << g0 << "," << g1 << endl;
    cout << " Filtering equivalence classes..." << endl;

    auto start_time = high_resolution_clock::now();
    
    // Use set of pairs to store mathematically unique PACP pairs
    set<pair<string, string>> unique_pairs;
    string line;
    int raw_count = 0;

    while (getline(fin, line)) {
        if (line.empty()) continue;
        raw_count++;
        
        size_t comma = line.find(',');
        if (comma == string::npos) continue;

        string a = get_canonical(line.substr(0, comma));
        string b = get_canonical(line.substr(comma + 1));

        // Sort pair to handle {A, B} == {B, A}
        if (a < b) unique_pairs.insert({a, b});
        else unique_pairs.insert({b, a});
    }
    fin.close();

    // Output results
    ofstream fout(out_path);
    for (const auto& p : unique_pairs) {
        fout << p.first << "," << p.second << "\n";
    }
    fout.close();

    auto end_time = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end_time - start_time);

    cout << "-------------------------------------------" << endl;
    cout << " Filter Report:" << endl;
    cout << " - Raw pairs processed: " << raw_count << endl;
    cout << " - Unique classes found: " << unique_pairs.size() << endl;
    cout << " - Time elapsed: " << duration.count() / 1000.0 << " s" << endl;
    cout << " - Saved to: " << out_path << endl;
    cout << "-------------------------------------------" << endl;

    return 0;
}