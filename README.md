# The ReadMe of FGo

## 0x01 Introduction

FGo is a probabilistic exponential *cut-the-loss* directed grey-box fuzzer based on [AFLGo](https://github.com/aflgo/aflgo). FGo terminates unreachable test cases early with exponentially increasing probability. Compared to other directed grey-box fuzzers, FGo makes full use of the unreachable information contained in iCFG and doesn't generate any additional overhead caused by reachability analysis. Moreover, it is easy to generalize to all Program Under Test (PUT). This strategy based on probability is perfectly adapted to the randomness of fuzzing.

The usage of FGo is similar to AFLGo. The only difference is that you should replace

```shell
$AFLGO/scripts/gen_distance_fast.py $SUBJECT $TMP_DIR xmllint
```

with

```shell
$FGO/distance/distance_generator.py $SUBJECT $TMP_DIR xmllint
```

in the step which generates distance files.

> Of course, all environment variables should be changed from AFLGO (or aflgo) to FGO (or fgo).

If you want to try some different *cut-the-loss* probability $p$, you can modify the parameter in `llvm_mode/afl-llvm-rt.o.c`.

```C
// llvm_mode/afl-llvm-rt.o.c: line 41-line 46

void noway() {
    srand(time(0));
    // FGo: p = 0.1
    if (rand() % 10 + 1 > 9) assert(false);
    return;
}
```

The value of $p$ equals the probability that `rand() % 10 + 1 > 9` holds. For example, you can set `rand() % 100 + 1 > 95` to get $p = 0.05$.

Read [our article](https://arxiv.org/abs/2307.05961) for more details.

## 0x02 The Fuzzing of min1 by FGo

### 1. Preparation

```shell
cd ~/obj-fgo
cp -rf ~/projects/libming .
mv libming min1
cd ~/obj-fgo/min1
git checkout 6f91d1a

export FGO=~/fgo
export SUBJECT=$PWD
mkdir obj-fgo
mkdir obj-fgo/temp
export TMP_DIR=$PWD/obj-fgo/temp

echo $'main.c:350\nmain.c:265\nblocktypes.c:145\nparser.c:3345\nparser.c:3302\nparser.c:3068' > $TMP_DIR/BBtargets.txt
```

### 2. First Compilation with BBtargets.txt

```shell
export CC=$FGO/afl-clang-fast
export CXX=$FGO/afl-clang-fast++
export COPY_CFLAGS=$CFLAGS
export COPY_CXXFLAGS=$CXXFLAGS
export ADDITIONAL="-targets=$TMP_DIR/BBtargets.txt -outdir=$TMP_DIR -flto -fuse-ld=gold -Wl,-plugin-opt=save-temps"
export CFLAGS="$COPY_CFLAGS $ADDITIONAL"
export CXXFLAGS="$COPY_CXXFLAGS $ADDITIONAL"
export LDFLAGS=-lpthread

./autogen.sh
cd ~/obj-fgo/min1/obj-fgo
../configure --disable-shared --disable-freetype
make clean
make

cat $TMP_DIR/BBnames.txt | rev | cut -d: -f2- | rev | sort | uniq > $TMP_DIR/BBnames2.txt && mv $TMP_DIR/BBnames2.txt $TMP_DIR/BBnames.txt
cat $TMP_DIR/BBcalls.txt | sort | uniq > $TMP_DIR/BBcalls2.txt && mv $TMP_DIR/BBcalls2.txt $TMP_DIR/BBcalls.txt
```

### 3. The Generation of distance.cfg.txt

```shell
$FGO/distance/distance_generator.py $SUBJECT/obj-fgo/util $TMP_DIR swftophp
```

### 4. Second Compilation with distance.cfg.txt

```shell
export CFLAGS="$COPY_CFLAGS -distance=$TMP_DIR/distance.cfg.txt"
export CXXFLAGS="$COPY_CXXFLAGS -distance=$TMP_DIR/distance.cfg.txt"

../configure --disable-shared --disable-freetype
make clean
make
```

### 5. Seed

```shell
mkdir in
wget -P in http://condor.depaul.edu/sjost/hci430/flash-examples/swf/bumble-bee1.swf
```

### 6. Fuzzing  

```shell
cd ~/obj-fgo/min1/obj-fgo
~/fgo/afl-fuzz -m none -z exp -c 120m -i in -o out -t 5000+ ./util/swftophp @@
```
