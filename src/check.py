import numpy as np
import matplotlib.pyplot as plt

def str_to_seq(s):
    """將 + - 字串轉換為 1, -1 陣列"""
    return np.array([1 if c == '+' else -1 for c in s.strip()])

def periodic_acf(seq):
    """計算週期自相關函數"""
    L = len(seq)
    r = np.zeros(L, dtype=int)
    for k in range(L):
        # 循環移位計算內積
        shifted = np.roll(seq, -k)
        r[k] = np.sum(seq * shifted)
    return r

# 你的序列
seq_a_str = "+-++--++++---++-+---+-++++--+--+++-+++--+++-"
seq_b_str = "-++-++-+++--+-+-+++++-+---+-----+-++-+++++--"

# 轉換
a = str_to_seq(seq_a_str)
b = str_to_seq(seq_b_str)
L = len(a)

# 計算 ACF
rho_a = periodic_acf(a)
rho_b = periodic_acf(b)
sum_rho = rho_a + rho_b

# 分析旁瓣 (k=1 到 L-1)
sidelobes = sum_rho[1:]
max_psl = np.max(np.abs(sidelobes))
bad_points = [k+1 for k, val in enumerate(sidelobes) if abs(val) > 4]

print(f"=== Verification for L={L} ===")
print(f"Sequence A: {seq_a_str}")
print(f"Sequence B: {seq_b_str}")
print("-" * 30)
print(f"PACF Sum (Full): {sum_rho}")
print(f"PACF Sum (Sidelobes): {sidelobes}")
print("-" * 30)
print(f"Max PSL: {max_psl}")

if max_psl <= 4:
    print("\n✅ SUCCESS! This is a valid (L, 4)-PQCP.")
    # 檢查是否為 Optimal (是否只有 4 而沒有其他雜訊?)
    # Project 2 定義: (L, 4)-PQCP 允許有多個 4
    count_4 = np.sum(np.abs(sidelobes) == 4)
    print(f"Number of peaks with magnitude 4: {count_4}")
else:
    print(f"\n❌ FAIL. Violations at shifts k={bad_points}")