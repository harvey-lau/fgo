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
llvm11_dir="$dependency_dir/llvm-11.0.0.obj"
llvm14_dir="$dependency_dir/llvm-14.0.0.obj"
z3_dir="$dependency_dir/z3.obj"

llvm11_llvm_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/llvm-11.0.0.src.tar.xz"
llvm11_clang_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/clang-11.0.0.src.tar.xz"
llvm11_compiler_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/compiler-rt-11.0.0.src.tar.xz"
llvm11_libcxx_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/libcxx-11.0.0.src.tar.xz"
llvm11_libcxxabi_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-11.0.0/libcxxabi-11.0.0.src.tar.xz"

llvm11_llvm_file="$llvm11_dir/llvm-11.0.0.src.tar.xz"
llvm11_clang_file="$llvm11_dir/clang-11.0.0.src.tar.xz"
llvm11_compiler_file="$llvm11_dir/compiler-rt-11.0.0.src.tar.xz"
llvm11_libcxx_file="$llvm11_dir/libcxx-11.0.0.src.tar.xz"
llvm11_libcxxabi_file="$llvm11_dir/libcxxabi-11.0.0.src.tar.xz"

llvm11_llvm_pre_dir="$llvm11_dir/llvm-11.0.0.src"
llvm11_clang_pre_dir="$llvm11_dir/clang-11.0.0.src"
llvm11_compiler_pre_dir="$llvm11_dir/compiler-rt-11.0.0.src"
llvm11_libcxx_pre_dir="$llvm11_dir/libcxx-11.0.0.src"
llvm11_libcxxabi_pre_dir="$llvm11_dir/libcxxabi-11.0.0.src"

llvm11_llvm_dir="$llvm11_dir/build/llvm_tools/llvm-11.0.0.src"
llvm11_clang_dir="$llvm11_dir/build/llvm_tools/llvm-11.0.0.src/tools/clang"
llvm11_compiler_dir="$llvm11_dir/build/llvm_tools/llvm-11.0.0.src/projects/compiler-rt"
llvm11_libcxx_dir="$llvm11_dir/build/llvm_tools/llvm-11.0.0.src/projects/libcxx"
llvm11_libcxxabi_dir="$llvm11_dir/build/llvm_tools/llvm-11.0.0.src/projects/libcxxabi"

llvm14_prebuilt_url="https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz"
llvm14_prebuilt_file="$dependency_dir/llvm-14.0.0-prebuilt.tar.xz"
llvm14_pre_dir="$dependency_dir/clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04"

z3_prebuilt_url="https://github.com/Z3Prover/z3/releases/download/z3-4.8.8/z3-4.8.8-x64-ubuntu-16.04.zip"
z3_prebuilt_file="$dependency_dir/z3-4.8.8-x64-ubuntu-16.04.zip"
z3_pre_dir="$dependency_dir/z3-4.8.8-x64-ubuntu-16.04"

jobs=$(nproc --all)
jobs=$(($jobs))
if [ $jobs -gt 1 ]; then
    jobs=$(($jobs - 1))
fi

if ! command -v tar &>/dev/null; then
    echo "Failed to find tar. Please install it."
    exit 1
fi

if ! command -v wget &>/dev/null; then
    echo "Failed to find wget. Please install it."
    exit 1
fi

if ! command -v unzip &>/dev/null; then
    echo "Failed to find unzip. Please install it."
    exit 1
fi

download_package() {
    local url="$1"
    local output_file="$2"

    wget $url -O $output_file -q
}

uncompress_package() {
    local file="$1"
    local out_dir="$2"

    tar xf "$file" -C "$out_dir"
}

# LLVM 11.0.0
if [ -d "$llvm11_clang_dir" ]; then
    echo "The LLVM 11.0.0 has been downloaded into $llvm11_dir."
else
    rm -rf "$llvm11_dir"
    mkdir "$llvm11_dir"

    echo "Downloading LLVM 11.0.0 source code..."
    download_package "$llvm11_llvm_url" "$llvm11_llvm_file"
    download_package "$llvm11_clang_url" "$llvm11_clang_file"
    download_package "$llvm11_compiler_url" "$llvm11_compiler_file"
    download_package "$llvm11_libcxx_url" "$llvm11_libcxx_file"
    download_package "$llvm11_libcxxabi_url" "$llvm11_libcxxabi_file"

    echo "Uncompressing source code packages..."
    uncompress_package "$llvm11_llvm_file" "$llvm11_dir"
    uncompress_package "$llvm11_clang_file" "$llvm11_dir"
    uncompress_package "$llvm11_compiler_file" "$llvm11_dir"
    uncompress_package "$llvm11_libcxx_file" "$llvm11_dir"
    uncompress_package "$llvm11_libcxxabi_file" "$llvm11_dir"
    rm -f "$llvm11_llvm_file" "$llvm11_clang_file" "$llvm11_compiler_file" "$llvm11_libcxx_file" "$llvm11_libcxxabi_file"

    mkdir -p "$llvm11_dir/build/llvm_tools"
    mv "$llvm11_llvm_pre_dir" "$llvm11_llvm_dir"
    mv "$llvm11_clang_pre_dir" "$llvm11_clang_dir"
    mv "$llvm11_compiler_pre_dir" "$llvm11_compiler_dir"
    mv "$llvm11_libcxx_pre_dir" "$llvm11_libcxx_dir"
    mv "$llvm11_libcxxabi_pre_dir" "$llvm11_libcxxabi_dir"
fi

# LLVM 14.0.0
if [ -d "$llvm14_dir/bin" ]; then
    echo "The LLVM 14.0.0 has been downloaded into $llvm14_dir."
else
    rm -rf "$llvm14_dir"
    
    echo "Downloading LLVM 14.0.0 prebuilt package..."
    download_package "$llvm14_prebuilt_url" "$llvm14_prebuilt_file"

    echo "Uncompressing prebuilt package..."
    uncompress_package "$llvm14_prebuilt_file" "$dependency_dir"
    rm -f "$llvm14_prebuilt_file"

    mv "$llvm14_pre_dir" "$llvm14_dir"
fi

# Z3
if [ -d "$z3_dir/bin" ]; then
    echo "The Z3 has been downloaded into $llvm14_dir."
else
    rm -rf "$z3_dir"
    
    echo "Downloading Z3 14.0.0 prebuilt package..."
    download_package "$z3_prebuilt_url" "$z3_prebuilt_file"

    echo "Uncompressing prebuilt package..."
    unzip -q "$z3_prebuilt_file" -d "$dependency_dir"
    rm -f "$z3_prebuilt_file"

    mv "$z3_pre_dir" "$z3_dir"
fi

echo
echo "Now, you can build the dependencies!"