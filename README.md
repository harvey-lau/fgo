# FGo

## Description

## Build

You can run the `./build.sh` to quickly build the whole project. Besides, you can also follow the following instructions to build the project step by setp.

Firstly, enter the [`Dependency`](./Dependency/) directory, build the dependencies. You can run the `download-libraries.sh`, `build-llvm11.sh`, `build-svf.sh` and `build-jsoncpp.sh` one by one to download the LLVM prebuilt binaries and source code, Z3 prebuilt binaries and build these dependencies. You can find more details in [`Dpendency-README`](./Dependency/README.md).

Secondly, enter the [`Analyzer`](./Analyzer/) directory, build the distance analyzer via `build-analyzer.sh`. You can find more details in [`Analyzer-README`](./Analyzer/README.md).

Finally, enter the [`AFL-Fuzz`](./AFL-Fuzz/) to build AFL fuzzing tools using `make`.

## Usage

## Example