#!/bin/bash

set -e

env_flag=0
build_type="Release"

if [ $# -gt 0 ]; then
    arg="$1"
    if [[ "$arg" == "help" ]]; then
        echo -e "$0 [Release|Debug] [env]"
        exit 0
    fi

    for arg in "$@"; do
        if [[ "$arg" =~ ^[Ee][Nn][Vv]$ ]]; then
            env_flag=1
        elif [[ "$arg" =~ ^[Dd]ebug$ ]]; then
            build_type="Debug"
        elif [[ "$arg" =~ ^[Rr]elease$ ]]; then
            build_type="Release"
        else
            echo "Unknown argument option '$arg'"
            exit 1
        fi
    done
fi

if [ "$env_flag" = "1" ]; then
    echo "Using environment variables specified by you"
else
    dependency_dir="$(readlink -f "$(dirname "$0")/../Dependency")"
    echo "Using libraries in ../Dependency"
    export SVF_DIR="$dependency_dir/SVF"
    export LLVM_DIR="$dependency_dir/llvm-14.0.0.obj"
    export Z3_DIR="$dependency_dir/z3.obj"
    export JSONCPP_DIR="$dependency_dir/jsoncpp"
    export INDICATORS_DIR="$dependency_dir/indicators"
fi
export PATH=$LLVM_DIR/bin:$PATH
build_dir="${build_type}-build"

cmake -D CMAKE_BUILD_TYPE:STRING="${build_type}" \
    -DBUILD_SHARED_LIBS=off                      \
    -B "${build_dir}"

cmake --build "${build_dir}"