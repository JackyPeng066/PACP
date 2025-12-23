#include <iostream>
#include <vector>
#include <cstdint>
#include <string>
#include <cmath>

using namespace std;

string bits_to_str(uint32_t bits, int L) {
    string s = "";
    for (int i = 0; i < L; ++i) s += (bits & (1U << i)) ? '+' : '-';
    return s;
}

int main(int argc, char* argv[]) {
    if (argc < 4) return 1;
    int L = atoi(argv[1]);
    int target_sum = atoi(argv[3]); 
    uint32_t mask = (1U << L) - 1;

    vector<uint32_t> poolB;
    uint32_t temp;
    FILE* fb = fopen(argv[2], "rb");
    if (!fb) return 1;
    while (fread(&temp, sizeof(uint32_t), 1, fb)) poolB.push_back(temp);
    fclose(fb);

    uint32_t a;
    while (fread(&a, sizeof(uint32_t), 1, stdin)) {
        for (uint32_t b : poolB) {
            bool ok = true;
            // 這裡就是 PDF 中最核心的定義：對所有位移 u，總和不能超過門檻
            for (int u = 1; u <= L / 2; ++u) {
                uint32_t rotA = ((a << u) | (a >> (L - u))) & mask;
                uint32_t rotB = ((b << u) | (b >> (L - u))) & mask;
                
                // 計算 A 和 B 的週期自相關
                int rA = L - 2 * __builtin_popcount(a ^ rotA);
                int rB = L - 2 * __builtin_popcount(b ^ rotB);

                // 嚴格判定：這一步出錯，結果就是錯的
                if (abs(rA + rB) > target_sum) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                cout << bits_to_str(a, L) << "," << bits_to_str(b, L) << "\n";
            }
        }
    }
    return 0;
}