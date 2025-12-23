#include <iostream>
#include <cstdint>
#include <cmath>

using namespace std;

int L, target_g, max_rho;
uint32_t current_bits = 0;

// 模擬 Tyler Lumsden 的即時剪枝：在填入過程中檢查
bool is_promising(int index) {
    if (index < 2) return true;
    for (int u = 1; u < index; ++u) {
        int partial_rho = 0;
        for (int i = 0; i + u < index; ++i) {
            bool bit1 = (current_bits >> i) & 1;
            bool bit2 = (current_bits >> (i + u)) & 1;
            partial_rho += (bit1 == bit2 ? 1 : -1);
        }
        // 若剩餘位數不足以將 rho 拉回 max_rho，則剪枝
        if (abs(partial_rho) - (L - index) > max_rho) return false;
    }
    return true;
}

void dfs(int index, int current_g) {
    if (current_g > target_g || (current_g + (L - index) < target_g)) return;

    if (index == L) {
        // 最終週期性檢查 (PACF)
        for (int u = 1; u <= L / 2; ++u) {
            int rho = 0;
            for (int i = 0; i < L; ++i) {
                bool b1 = (current_bits >> i) & 1;
                bool b2 = (current_bits >> ((i + u) % L)) & 1;
                rho += (b1 == b2 ? 1 : -1);
            }
            if (abs(rho) > max_rho) return;
        }
        fwrite(&current_bits, sizeof(uint32_t), 1, stdout);
        return;
    }

    // 填入 + (bit 1)
    current_bits |= (1U << index);
    if (is_promising(index + 1)) dfs(index + 1, current_g + 1);
    
    // 填入 - (bit 0)
    current_bits &= ~(1U << index);
    if (is_promising(index + 1)) dfs(index + 1, current_g);
}