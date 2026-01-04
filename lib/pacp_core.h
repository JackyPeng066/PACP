#ifndef PACP_CORE_H
#define PACP_CORE_H

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <numeric>
#include <algorithm>

using Seq = std::vector<int>;

// Compute Aperiodic Auto-Correlation Function (ACF)
void compute_acf(const Seq& s, std::vector<int>& acf_out);

// [New] Compute Periodic ACF (for Goal 3)
void compute_periodic_acf(const Seq& S, std::vector<int>& out_acf);

// Peak Sidelobe Level (PSL)
int calc_psl(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L);

// Mean Squared Error (MSE) Cost Function
long long calc_mse_cost(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L, int target_val);

// Canonical Representation for Deduplication
std::string get_canonical_repr(const Seq& s);

// File I/O
void save_result_list(const std::string& filename, const std::vector<std::pair<Seq, Seq>>& results, int L, int psl);
bool load_result(const std::string& filename, Seq& a, Seq& b);

// Helpers
void int_to_seq(int val, int L, Seq& s);
void rotate_seq_left(Seq& s, int k);

#endif