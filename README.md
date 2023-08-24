# The ReadMe of FGo

FGo is a probabilistic exponential *cut-the-loss* directed grey-box fuzzer based on [AFLGo](https://github.com/aflgo/aflgo). FGo terminates unreachable test cases early with exponentially increasing probability. Compared to other directed grey-box fuzzers, FGo makes full use of the unreachable information contained in iCFG and doesn‘t generate any additional overhead caused by reachability analysis. Moreover, it is easy to generalize to all PUT. This strategy based on probability is perfectly adapted to the randomness of fuzzing.

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
