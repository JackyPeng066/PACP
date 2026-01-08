/*
    PACP/PQCP Interactive Analyzer (Full Spectrum)
    Ref: Optimal Binary Periodic Almost-Complementary Pairs
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
const std::string C_RED = "\033[1;31m";   // Error / Fail
const std::string C_GRN = "\033[1;32m";   // Perfect / Optimal
const std::string C_YEL = "\033[1;33m";   // Near Optimal / Peak
const std::string C_CYN = "\033[1;36m";   // Headers
const std::string C_WHT = "\033[1;37m";   // Title
const std::string C_MAG = "\033[1;35m";   // Special Info

// --- Helper: Parse string to vector (1/-1) ---
std::vector<int> parse_single_sequence(std::string raw) {
    std::vector<int> seq;
    for (size_t i = 0; i < raw.length(); ++i) {
        char c = raw[i];
        if (c == '+') seq.push_back(1);
        else if (c == '-') seq.push_back(-1);
        else if (c == '1') {
            if (i > 0 && raw[i-1] == '-') { /* handled */ } 
            else { seq.push_back(1); }
        }
        else if (c == '0') { 
            // Support for binary 0/1 notation if needed, mapping 0->1, 1->-1 or vice versa
            // Assuming standard + - format primarily based on your previous input
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
    size_t L_size = A.size();
    int L = (int)L_size;

    if (B.size() != L_size) {
        std::cout << C_RED << "[Error] Length mismatch! A=" << L_size << ", B=" << B.size() << C_RST << std::endl;
        return;
    }

    auto rhoA = compute_periodic_acf(A);
    auto rhoB = compute_periodic_acf(B);
    std::vector<int> sum(L);

    bool is_even = (L % 2 == 0);
    bool is_optimal_candidate = true;
    bool is_near_optimal_candidate = true; // Only for Even
    
    int max_sidelobe = 0;

    std::cout << "\n" << C_CYN << "--- PACP ANALYSIS (L=" << L << ") ---" << C_RST << std::endl;
    if (is_even) {
        std::cout << "Mode: " << C_MAG << "EVEN" << C_RST << " (Target: 0 inside, 4 at L/2) " << std::endl;
    } else {
        std::cout << "Mode: " << C_MAG << "ODD" << C_RST << "  (Target: 2 everywhere) " << std::endl;
    }
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "  u |   rhoA |   rhoB |    Sum | Status   " << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    // Show FULL spectrum from 0 to L-1
    for (int u = 0; u < L; ++u) { 
        sum[u] = rhoA[u] + rhoB[u];
        int val = std::abs(sum[u]);
        
        std::string status = "";
        std::string color = C_RST;

        if (u == 0) {
            // Main Lobe Check
            if (sum[u] == 2 * L) {
                status = "MAIN";
                color = C_CYN;
            } else {
                status = "ERR";
                color = C_RED;
                is_optimal_candidate = false;
            }
        } else {
            // Sidelobe Checks
            if (val > max_sidelobe) max_sidelobe = val;

            if (is_even) {
                // --- EVEN LENGTH LOGIC ---
                if (u == L / 2) {
                    // Theorem 2: Must be >= 4. Optimal == 4.
                    if (val == 4) {
                        status = "OPT"; // Optimal Peak
                        color = C_GRN;
                        // For Near-Optimal, this must be > 4, so if it's 4, near-opt flag fails strictly speaking?
                        // Usually Near-Optimal implies strictly > 4.
                        is_near_optimal_candidate = false; 
                    } else if (val > 4 && val % 4 == 0) {
                        status = "NEAR"; // Near Optimal Peak
                        color = C_YEL;
                        is_optimal_candidate = false; // Cannot be optimal if > 4
                    } else {
                        status = "FAIL";
                        color = C_RED;
                        is_optimal_candidate = false;
                        is_near_optimal_candidate = false;
                    }
                } else {
                    // ZCZ Region: Must be 0
                    if (val == 0) {
                        status = "ZERO";
                        color = C_GRN; // Good
                    } else {
                        status = "NOISE";
                        color = C_RED; // Bad for Optimal PACP
                        is_optimal_candidate = false;
                        is_near_optimal_candidate = false;
                    }
                }
            } else {
                // --- ODD LENGTH LOGIC ---
                if (val == 2) {
                    status = "OPT";
                    color = C_GRN;
                } else {
                    status = "FAIL"; // E.g. 6, 10...
                    color = C_RED;
                    is_optimal_candidate = false;
                }
            }
        }

        printf("%3d | %6d | %6d | %6d | %s%s%s\n", 
               u, rhoA[u], rhoB[u], sum[u], color.c_str(), status.c_str(), C_RST.c_str());
    }
    std::cout << "-------------------------------------------" << std::endl;

    // --- Final Verdict ---
    std::cout << "Verdict: ";
    if (is_even) {
        if (is_optimal_candidate) {
            std::cout << C_GRN << "[PERFECT] Optimal Binary PACP (L, 4)" << C_RST << std::endl;
            std::cout << "         Satisfies Theorem 2 & Def 1 (Page 13) " << std::endl;
        } else if (is_near_optimal_candidate) {
            std::cout << C_YEL << "[GOOD] Near-Optimal PACP" << C_RST << std::endl;
            std::cout << "         Max ZCZ, but peak > 4 at L/2 " << std::endl;
        } else {
            // Check for generalized PQCP (allow noise)
            std::cout << C_RED << "[FAIL] Not a Strict PACP." << C_RST << std::endl;
            std::cout << "         Max Sidelobe Level (PSL): " << max_sidelobe << std::endl;
            if (max_sidelobe <= 4) {
                 std::cout << "         (However, it is a valid (L, 4)-PQCP)" << std::endl;
            }
        }
    } else {
        // Odd
        if (is_optimal_candidate) {
            std::cout << C_GRN << "[PERFECT] Optimal Odd PACP" << C_RST << std::endl;
            std::cout << "         Magnitude is 2 everywhere (Page 13) " << std::endl;
        } else {
            std::cout << C_RED << "[FAIL] Not Optimal." << C_RST << " Max Sidelobe: " << max_sidelobe << std::endl;
        }
    }
    std::cout << std::endl;
}

int main() {
    std::string line;
    std::cout << C_WHT << "=== PACP/PQCP Full Spectrum Analyzer ===" << C_RST << std::endl;
    std::cout << "Supports Optimal PACP check for Even & Odd lengths." << std::endl;
    std::cout << "Paste your pair below (e.g. ++-+,+--+ or 1 1 -1 1, 1 -1 -1 1)" << std::endl;
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