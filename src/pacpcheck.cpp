#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>
#include <algorithm>

using namespace std;

// Periodic Autocorrelation Function
int calc_pacf(const string& s, int u, int L) {
    int sum = 0;
    for (int i = 0; i < L; ++i) {
        int a = (s[i] == '+') ? 1 : -1;
        int b = (s[(i + u) % L] == '+') ? 1 : -1;
        sum += a * b;
    }
    return sum;
}

int main() {
    string input;
    cout << "Enter Pair (Format: SEQ_A,SEQ_B): " << endl;
    cin >> input;

    // 1. Parse Input by comma
    size_t comma_pos = input.find(',');
    if (comma_pos == string::npos) {
        cout << "Error: Invalid format. Use 'A,B'" << endl;
        return 1;
    }

    string sA = input.substr(0, comma_pos);
    string sB = input.substr(comma_pos + 1);

    if (sA.length() != sB.length()) {
        cout << "Error: Length mismatch! A=" << sA.length() << " B=" << sB.length() << endl;
        return 1;
    }

    int L = sA.length();
    // Dynamic Parity Logic from PDF
    int target = (L % 2 != 0) ? 2 : 4;

    cout << "\n--- PACP ANALYSIS (L=" << L << ") ---" << endl;
    cout << "Mode: " << (L % 2 != 0 ? "Odd (Target <= 2)" : "Even (Target <= 4)") << endl;
    cout << "-------------------------------------------" << endl;
    cout << setw(4) << "u" << " | " << setw(6) << "rhoA" << " | " << setw(6) << "rhoB" << " | " << setw(6) << "Sum" << " | Status" << endl;
    cout << "-------------------------------------------" << endl;

    bool is_valid = true;
    for (int u = 1; u <= L / 2; ++u) {
        int rA = calc_pacf(sA, u, L);
        int rB = calc_pacf(sB, u, L);
        int total = rA + rB;
        bool ok = abs(total) <= target;
        if (!ok) is_valid = false;

        cout << setw(4) << u << " | " 
             << setw(6) << rA << " | " 
             << setw(6) << rB << " | " 
             << setw(6) << total << " | " 
             << (ok ? "OK" : "FAIL (!!!)") << endl;
    }

    cout << "-------------------------------------------" << endl;
    if (is_valid) {
        cout << ">>> SUCCESS: This is a valid PACP pair." << endl;
    } else {
        cout << ">>> FAILED: Does not meet PACP criteria." << endl;
    }
    
    return 0;
}