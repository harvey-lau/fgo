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
jsoncpp_home="$dependency_dir/jsoncpp"

if [ ! -d "$jsoncpp_home" ]; then
    echo "Failed to find jsoncpp under $dependency_dir. Please check whether you have cloned this repository."
    exit 1
fi

jobs=$(nproc --all)
jobs=$(($jobs))
if [ $jobs -gt 1 ]; then
    jobs=$(($jobs - 1))
fi

build_jsoncpp() {
    build_type="$1"
    build_dir="${build_type}-build"

    pushd "$jsoncpp_home" >/dev/null

    rm -rf "$build_dir"

    cmake -D CMAKE_BUILD_TYPE:STRING="${build_type}" \
        -DBUILD_SHARED_LIBS=on                       \
        -DBUILD_STATIC_LIBS=on                       \
        -B "${build_dir}"

    ## build
    echo "Parallel Jobs: $jobs"
    cmake --build "${build_dir}" -j $jobs

    popd >/dev/null
}

build_jsoncpp "Release"
build_jsoncpp "Debug"

echo
echo "Now, you can move to the next stage!"