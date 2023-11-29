#!/usr/bin/env bash

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
svf_home="${dependency_dir}/SVF"
llvm_dir="$dependency_dir/llvm-14.0.0.obj"
z3_dir="$dependency_dir/z3.obj"

if [ ! -d "$svf_home" ]; then
    echo "Failed to find SVF under $dependency_dir. Please check whether you have cloned this repository."
    exit 1
fi
if [ ! -d "$llvm_dir" ]; then
    echo "Failed to find LLVM 14.0.0 under $dependency_dir. Please download LLVM 14.0.0 first."
    exit 1
fi
if [ ! -d "$z3_dir" ]; then
    echo "Failed to find Z3 under $dependency_dir. Please download Z3 first."
    exit 1
fi
if [ ! -d "$llvm_dir/bin" ]; then
    echo "Failed to find binaries under $llvm_dir. Please check whether you have downloaded LLVM 14.0.0 prebuilt packages."
    exit 1
fi

jobs=$(nproc --all)
jobs=$(($jobs))
if [ $jobs -gt 1 ]; then
    jobs=$(($jobs - 1))
fi

build_svf() {
    build_type="$1"
    build_dir="${build_type}-build"

    pushd "$svf_home" >/dev/null

    rm -rf "$build_dir"

    export PATH=$llvm_dir/bin:$PATH
    export LLVM_DIR=$llvm_dir
    export Z3_DIR=$z3_dir

    cmake -D CMAKE_BUILD_TYPE:STRING="${build_type}" \
        -DSVF_ENABLE_ASSERTIONS:BOOL=true            \
        -DSVF_SANITIZE="${SVF_SANITIZER}"            \
        -DBUILD_SHARED_LIBS=off                      \
        -S "${svf_home}" -B "${build_dir}"

    ## build
    echo "Parallel Jobs: $jobs"
    cmake --build "${build_dir}" -j $jobs

    popd >/dev/null
}

build_svf "Release"
build_svf "Debug"

echo
echo "Now, you can move to the next stage!"