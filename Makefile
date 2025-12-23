# ============================================
# Compiler Settings
# ============================================
CXX = g++
CXXFLAGS = -O3 -std=c++17 -Wall -Wextra
# LDFLAGS = -lfftw3 -lm

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
# Build Library Objects (lib/*.cpp → bin/*.o)
# ============================================
$(BINDIR)/%.o: $(LIBDIR)/%.cpp $(LIBDIR)/%.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================
# Build Executables (src/*.cpp → bin/*)
# ============================================
$(BINDIR)/%: $(SRCDIR)/%.cpp $(LIBOBJ)
	$(CXX) $(CXXFLAGS) $< $(LIBOBJ) $(LDFLAGS) -o $@

# ============================================
# Clean
# ============================================
clean:
	rm -f $(BINDIR)/*.o $(BINDIR)/*
