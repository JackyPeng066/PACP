/*
    PQCP Interactive Analyzer (Fixed)
    Path: src/pqcpcheck.cpp
    Usage: ./bin/pqcpcheck
*/

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <algorithm>

// --- Color Codes ---
const std::string C_RST = "\033[0m";
const std::string C_RED = "\033[1;31m";
const std::string C_GRN = "\033[1;32m";
const std::string C_YEL = "\033[1;33m";
const std::string C_CYN = "\033[1;36m";
const std::string C_WHT = "\033[1;37m";

// --- Helper: Parse string to vector (1/-1) ---
std::vector<int> parse_single_sequence(std::string raw) {
    std::vector<int> seq;
    for (size_t i = 0; i < raw.length(); ++i) {
        char c = raw[i];
        if (c == '+') seq.push_back(1);
        else if (c == '-') seq.push_back(-1);
        else if (c == '1') {
            // Simple handler for numeric input like "1, -1"
            if (i > 0 && raw[i-1] == '-') { 
                // Handled by '-' check
            } else {
                seq.push_back(1);
            }
        }
    }
    return seq;
}

// Robust input splitter
bool parse_input_line(std::string line, std::vector<int>& A, std::vector<int>& B) {
    std::replace(line.begin(), line.end(), ',', ' ');
    std::stringstream ss(line);
    std::string segment;
    std::vector<std::string> parts;
    
    while (ss >> segment) {
        parts.push_back(segment);
    }

    if (parts.size() == 2) {
        A = parse_single_sequence(parts[0]);
        B = parse_single_sequence(parts[1]);
        return true;
    }
    return false;
}

// --- Math Core ---
std::vector<int> compute_periodic_acf(const std::vector<int>& s) {
    int L = (int)s.size();
    std::vector<int> rho(L, 0);
    for (int u = 0; u < L; ++u) {
        for (int i = 0; i < L; ++i) {
            rho[u] += s[i] * s[(i + u) % L];
        }
    }
    return rho;
}

void analyze_pair(const std::vector<int>& A, const std::vector<int>& B) {
    // [Fix] Use size_t to match .size() return type
    size_t L_size = A.size();
    int L = (int)L_size; // Keep an int version for logic usage if needed

    if (B.size() != L_size) {
        std::cout << C_RED << "[Error] Length mismatch! A=" << L_size << ", B=" << B.size() << C_RST << std::endl;
        return;
    }

    auto rhoA = compute_periodic_acf(A);
    auto rhoB = compute_periodic_acf(B);
    std::vector<int> sum(L);

    bool strict_pqcp = true;
    int nonzero_cnt = 0;
    int max_sidelobe = 0;
    
    std::cout << "\n" << C_CYN << "--- PACP ANALYSIS (L=" << L << ") ---" << C_RST << std::endl;
    std::cout << "Mode: Even (Target <= 4)" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "   u |   rhoA |   rhoB |    Sum | Status   " << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    for (int u = 1; u < L; ++u) { 
        sum[u] = rhoA[u] + rhoB[u];
        int val = std::abs(sum[u]);
        
        std::string status = "OK";
        std::string color = C_GRN;

        if (val > max_sidelobe) max_sidelobe = val;

        if (val != 0) {
            nonzero_cnt++;
            if (val != 4) {
                strict_pqcp = false;
                status = "NOISE";
                color = C_RED;
            } else {
                status = "PEAK"; 
                color = C_YEL;
            }
        }

        printf("%4d | %6d | %6d | %6d | %s%s%s\n", 
               u, rhoA[u], rhoB[u], sum[u], color.c_str(), status.c_str(), C_RST.c_str());
    }
    std::cout << "-------------------------------------------" << std::endl;

    int sum_0 = rhoA[0] + rhoB[0];
    if (sum_0 != 2 * L) {
        std::cout << C_RED << "[CRITICAL] Main Lobe Error! Sum(0)=" << sum_0 << " (Expected " << 2*L << ")" << C_RST << std::endl;
        return;
    }

    std::cout << "Result: ";
    if (strict_pqcp && nonzero_cnt == 2 && max_sidelobe == 4) {
        std::cout << C_GRN << "[VICTORY] Valid (L,4)-PQCP!" << C_RST << std::endl;
    } else if (max_sidelobe <= 4) {
        std::cout << C_YEL << "[CLOSE] Low PSL but not strict PQCP (Peaks: " << nonzero_cnt << ")" << C_RST << std::endl;
    } else {
        std::cout << C_RED << "[FAIL] PSL=" << max_sidelobe << " (Max > 4)" << C_RST << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::string line;
    std::cout << C_WHT << "=== PQCP Interactive Checker ===" << C_RST << std::endl;
    std::cout << "Paste your pair below (Format: ++-+,+--+)" << std::endl;
    std::cout << "Type 'exit' to quit.\n" << std::endl;

    while (true) {
        std::cout << C_CYN << "Enter Pair: " << C_RST;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;

        std::vector<int> A, B;
        if (parse_input_line(line, A, B)) {
            analyze_pair(A, B);
        } else {
            std::cout << C_RED << "Parse error! Use comma to separate A and B." << C_RST << std::endl;
        }
    }
    return 0;
}