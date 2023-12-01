#!/bin/bash

set -e

os="$(uname)"
arch="$(uname -m)"

if [[ "$os" != "Linux" ]]; then
    echo "The OS $os is not supported!"
    exit 1
fi

if [[ "$arch" != "x86_64" ]]; then
    echo "The architecture $arch is not supported!"
    exit 1
fi

dependency_dir="$(dirname "$(readlink -f "$0")")"
base_dir="$dependency_dir/llvm-11.0.0.obj/build/llvm-tools"
src_dir="$base_dir/llvm-11.0.0.src"

install_dir="$dependency_dir/llvm-11.0.0.obj"
binary_dir="$install_dir/bin"
clang_binary="$binary_dir/clang"
clangpp_binary="$binary_dir/clang++"

jobs=$(nproc --all)
jobs=$(($jobs))
if [ $jobs -gt 1 ]; then
    jobs=$(($jobs - 1))
fi

if ! command -v gcc &> /dev/null; then
    echo "Failed to find gcc. Please install it."
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "Failed to find gcc. Please install it."
    exit 1
fi

if [ -f "$clang_binary" ]; then
    echo "LLVM 11.0.0 has been built into $install_dir and the binaries can be found in $binary_dir."
    exit 0
fi

if [ ! -d "$src_dir" ]; then
    echo "LLVM 11.0.0 source code was not found. Please run './download-libraries.sh' first."
    exit 1
fi

pushd "$install_dir" >/dev/null
echo "Removing miscellaneous..."
rm -rf bin include lib libexec share
popd >/dev/null

pushd "$base_dir" >/dev/null

rm -rf build-llvm

mkdir -p build-llvm/llvm
cd build-llvm/llvm
cmake -G "Ninja" \
      -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_INSTALL_PREFIX="$install_dir" \
      -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXX_ENABLE_STATIC_ABI_LIBRARY=ON \
      -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86" \
      -DLLVM_BINUTILS_INCDIR=/usr/include "$src_dir"
ninja -j$jobs
ninja install

if [ ! -f "$clang_binary" ] || [ ! -f "$clangpp_binary" ]; then
    echo "Unexpected error: failed to build LLVM. Please run this script again."
    exit 1
fi

popd >/dev/null

echo
echo "Now, you can move to the next stage!"