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

jobs=$(nproc --all)
jobs=$(($jobs))
if [ $jobs -gt 1 ]; then
    jobs=$(($jobs - 1))
fi

dependency_dir="$(dirname "$(readlink -f "$0")")"

llvm_lib_dir="$dependency_dir/llvm-14.0.0.obj/lib"
if [ ! -d "$llvm_lib_dir" ]; then
    echo "Failed to find the LLVM 14.0.0. Please download the prebuilt package first!"
    exit 1
fi

echo "WARNING: You are ready to build the LLVM project. It may take long time."
echo "You have $(nproc --all) CPU cores and the building task will perform $jobs concurrent tasks."
read -p "Are you sure to continue? (y/n): " response

if [ "$response" == "y" ]; then
    echo "Allright, we continue our tasks..."
else
    echo "Exit"
    exit 1
fi

version_number="14.0.0"
install_dir="$dependency_dir/temp-llvm-${version_number}.obj"
src_dir="$dependency_dir/temp-llvm-${version_number}.src"
build_dir="$dependency_dir/temp-llvm-build"

mkdir -p "$install_dir"
rm -rf "$src_dir"

wget "https://jy-static-res.oss-cn-wulanchabu.aliyuncs.com/llvm/llvm-project/releases/download/llvmorg-${version_number}/llvm-project-llvmorg-${version_number}.tar.gz"
tar xf "llvm-project-llvmorg-${version_number}.tar.gz"
rm -f "llvm-project-llvmorg-${version_number}.tar.gz"
mv "llvm-project-llvmorg-${version_number}" "$src_dir"
src_dir="$src_dir/llvm"

mkdir "$build_dir"
pushd "$build_dir" >/dev/null

    cmake -G "Unix Makefiles" \
        -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_INSTALL_PREFIX="$install_dir" \
        -DLLVM_ENABLE_PROJECTS="llvm" -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_TARGETS_TO_BUILD="X86" -DLLVM_BINUTILS_INCDIR=/usr/include \
        -DLLVM_ENABLE_PROJECTS="llvm" \
        "$src_dir"

    make -j$jobs
    make install

popd >/dev/null

cp -f "$install_dir/lib/LLVMgold.so" "$llvm_lib_dir"
cp -f "$install_dir/lib/libLTO.so.14" "$llvm_lib_dir"

pushd "$dependency_dir" >/dev/null
    rm -rf temp-llvm-*
popd >/dev/null

echo
echo "Now, you can move to the next stage!"