#!/usr/bin/env bash

echo "======================================================="
echo "   MSYS2 UCRT64 + make 一鍵安裝腳本"
echo "======================================================="

# --------------------------------------------------------
# 檢查是否已安裝 filesystem 套件（判斷是否已完成第一次更新）
# --------------------------------------------------------
pacman -Qi filesystem > /dev/null 2>&1
FIRST_UPDATE_DONE=$?

if [ $FIRST_UPDATE_DONE -ne 0 ]; then
    # =====================================================
    # 第一次階段：執行 -Syu，然後要求重開 shell
    # =====================================================
    echo "[STEP 1] 進行第一次系統更新：pacman -Syu"
    echo "-------------------------------------------------------"
    pacman -Syu --noconfirm

    echo
    echo "[INFO] 第一次更新完成！"
    echo "[INFO] 請關閉 MSYS2，重新啟動『MSYS2 UCRT64』後再次執行："
    echo "       bash setup_env.sh"
    echo "======================================================="
    exit 0
fi


# =========================================================
# 第二階段：已完成第一次更新
# 開始正式安裝 UCRT64 toolchain + make
# =========================================================
echo
echo "[STEP 2] 偵測到第一次更新已完成，開始安裝工具鏈 ..."
echo "-------------------------------------------------------"

echo "[INSTALL] 安裝 UCRT64 toolchain（gcc, g++, ld, ar, make 等）"
pacman -S --needed --noconfirm base-devel mingw-w64-ucrt-x86_64-toolchain

echo
echo "[INSTALL] 安裝 make（如果前面已含則跳過）"
pacman -S --needed --noconfirm make

echo
echo "[OPTIONAL] 如需 FFTW（Golay-main、PCP 搜尋可能用到），可安裝："
echo "           pacman -S mingw-w64-ucrt-x86_64-fftw"
echo

# --------------------------------------------------------
# 測試工具是否可用
# --------------------------------------------------------
echo "[TEST] 測試 gcc/g++/make 是否安裝成功 ..."
echo

echo "gcc 版本："
gcc --version 2>/dev/null || echo "gcc 不可用"

echo
echo "g++ 版本："
g++ --version 2>/dev/null || echo "g++ 不可用"

echo
echo "make 版本："
make --version 2>/dev/null || echo "make 不可用"

echo
echo "======================================================="
echo "  UCRT64 + make 安裝完成！"
echo "======================================================="
echo "  你現在可以在 MSYS2 UCRT64 環境下執行 make/gcc/g++"
echo "======================================================="
