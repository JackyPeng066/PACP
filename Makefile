# ============================================
# Compiler Settings
# ============================================
CXX = g++

# 基礎參數: C++17, 最高優化等級 O3, 顯示警告
BASE_FLAGS = -O3 -std=c++17 -Wall -Wextra

# [硬體加速核心]
# -march=native : 自動偵測當前 CPU (14700) 並啟用專屬指令集 (AVX2, POPCNT...)
# -flto         : Link Time Optimization (連結時優化)，跨檔案極致加速
# -funroll-loops: 激進的迴圈展開
ARCH_FLAGS = -march=native -flto -funroll-loops

CXXFLAGS = $(BASE_FLAGS) $(ARCH_FLAGS)
LDFLAGS = $(ARCH_FLAGS) 
# 如果有用到 fftw3 再解開下面這行
# LDFLAGS += -lfftw3 -lm

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
	mkdir -p $(BINDIR)

# ============================================
# Build Library Objects
# ============================================
$(BINDIR)/%.o: $(LIBDIR)/%.cpp $(LIBDIR)/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================
# Build Executables
# ============================================
# 特別針對 optimizer3 系列進行極致靜態連結，減少 runtime overhead
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