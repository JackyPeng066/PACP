# ============================================
# Compiler Settings
# ============================================
CXX = g++

# 基礎參數: C++17, O3, 顯示警告
# [調整] 加入 -Ofast: 這是 O3 的進階版，允許非標準的代數優化 (對於 PACF 這種純整數/簡單運算極快)
# [調整] 加入 -fno-stack-protector: 搜尋程式不需要防禦緩衝區溢位，移除它可以換取極微小的寄存器壓力減輕
BASE_FLAGS = -Ofast -std=c++17 -Wall -Wextra -fno-stack-protector

# [硬體加速核心]
# -march=native: 啟用 AVX2/POPCNT
# -fomit-frame-pointer: 釋放一個通用寄存器 (ebp/rbp) 給搜尋邏輯使用
ARCH_FLAGS = -march=native -flto -funroll-loops -fomit-frame-pointer -finline-functions

CXXFLAGS = $(BASE_FLAGS) $(ARCH_FLAGS)
LDFLAGS = $(ARCH_FLAGS) 

# ============================================
# Directory Settings
# ============================================
LIBDIR = lib
SRCDIR = src
BINDIR = bin

LIBSRC = $(wildcard $(LIBDIR)/*.cpp)
LIBOBJ = $(patsubst $(LIBDIR)/%.cpp, $(BINDIR)/%.o, $(LIBSRC))

SRCS = $(wildcard $(SRCDIR)/*.cpp)
EXES = $(patsubst $(SRCDIR)/%.cpp, $(BINDIR)/%, $(SRCS))

# ============================================
# Default Target
# ============================================
all: prepare $(EXES)

prepare:
	@mkdir -p $(BINDIR)

# ============================================
# Build Library Objects
# ============================================
$(BINDIR)/%.o: $(LIBDIR)/%.cpp $(LIBDIR)/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================
# Build Executables
# ============================================
# 特別針對 optimizer 系列進行編譯 (包含您之後可能的 optimizer_pqcp)
# [調整] 加入 -s 參數進行 Strip，移除符號表，縮小 Binary 體積並稍微加快載入
$(BINDIR)/optimizer_pqcp: $(SRCDIR)/optimizer_pqcp.cpp $(LIBOBJ)
	$(CXX) $(CXXFLAGS) $< $(LIBOBJ) $(LDFLAGS) -s -o $@

$(BINDIR)/optimizer3: $(SRCDIR)/optimizer3.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

# 其他程式的通用編譯規則
$(BINDIR)/%: $(SRCDIR)/%.cpp $(LIBOBJ)
	$(CXX) $(CXXFLAGS) $< $(LIBOBJ) $(LDFLAGS) -o $@

# ============================================
# Clean
# ============================================
clean:
	rm -f $(BINDIR)/*.o $(BINDIR)/*