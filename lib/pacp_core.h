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

void compute_acf(const Seq& s, std::vector<int>& acf_out);

// 計算 PSL (Peak Sidelobe Level): 回傳旁瓣合的最大絕對值
int calc_psl(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L);

// 計算 Cost (MSE): 針對目標值的均方誤差 (Target通常為2或0)
long long calc_mse_cost(const std::vector<int>& acf_a, const std::vector<int>& acf_b, int L, int target_val);

// 產生序列的標準型字串 (用於去重)
std::string get_canonical_repr(const Seq& s);

// 儲存結果 (只在最後呼叫一次)
void save_result_list(const std::string& filename, const std::vector<std::pair<Seq, Seq>>& results, int L, int psl);

// I/O
bool load_result(const std::string& filename, Seq& a, Seq& b);
void int_to_seq(int val, int L, Seq& s); // 移到這裡方便大家用

#endif