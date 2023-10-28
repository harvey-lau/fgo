# Build AFL fuzzing program and AFLGo instrumentation compilers


ifdef AFL_NO_X86
	MAKE_FLAGS ?= AFL_NO_X86=1
endif

ifdef AFL_TRACE_PC
	MAKE_FLAGS += AFL_TRACE_PC=1
endif

ifdef AFLGO_TRACING
	MAKE_FLAGS += AFLGO_TRACING=1
endif

ifdef USE_RUST
	MAKE_FLAGS += USE_RUST=1
endif

all: afl instr copy_files

afl:
	$(MAKE) -C afl-2.57b $(MAKE_FLAGS)

instr:
	$(MAKE) -C instrument $(MAKE_FLAGS)

copy_files:
	@echo "Copying from afl-2.57b"
	@[ -f "afl-2.57b/afl-cmin" ] && cp -f afl-2.57b/afl-cmin . || true
	@[ -f "afl-2.57b/afl-plot" ] && cp -f afl-2.57b/afl-plot . || true
	@[ -f "afl-2.57b/afl-whatsup" ] && cp -f afl-2.57b/afl-whatsup . || true
	@[ -f "afl-2.57b/Android.bp" ] && cp -f afl-2.57b/Android.bp . || true
	@[ -f "afl-2.57b/afl-analyze" ] && cp -f afl-2.57b/afl-analyze . || true
	@[ -f "afl-2.57b/afl-as" ] && cp -f afl-2.57b/afl-as . || true
	@[ -f "afl-2.57b/afl-fuzz" ] && cp -f afl-2.57b/afl-fuzz . || true
	@[ -f "afl-2.57b/afl-gcc" ] && cp -f afl-2.57b/afl-gcc . || true
	@[ -f "afl-2.57b/afl-gotcpu" ] && cp -f afl-2.57b/afl-gotcpu . || true
	@[ -f "afl-2.57b/afl-showmap" ] && cp -f afl-2.57b/afl-showmap . || true
	@[ -f "afl-2.57b/afl-tmin" ] && cp -f afl-2.57b/afl-tmin . || true
	@[ -f "afl-2.57b/afl-clang" ] && cp -f -P afl-2.57b/afl-clang . || true
	@[ -f "afl-2.57b/afl-clang++" ] && cp -f -P afl-2.57b/afl-clang++ . || true
	@[ -f "afl-2.57b/afl-g++" ] && cp -f -P afl-2.57b/afl-g++ . || true
	@[ -f "afl-2.57b/as" ] && cp -f -P afl-2.57b/as . || true
	@echo "Copying from instrument"
	@[ -f "instrument/aflgo-clang" ] && cp -f instrument/aflgo-clang . || true
	@[ -f "instrument/aflgo-pass.so" ] && cp -f instrument/aflgo-pass.so . || true
	@[ -f "instrument/aflgo-runtime.o" ] && cp -f instrument/aflgo-runtime.o . || true
	@[ -f "instrument/aflgo-runtime-64.o" ] && cp -f instrument/aflgo-runtime-64.o . || true
	@[ -f "instrument/aflgo-runtime-32.o" ] && cp -f instrument/aflgo-runtime-32.o . || true
	@[ -f "instrument/aflgo-clang++" ] && cp -f -P instrument/aflgo-clang++ . || true
	@[ -f "instrument/afl-clang-fast" ] && cp -f -P instrument/afl-clang-fast . || true
	@[ -f "instrument/afl-clang-fast++" ] && cp -f -P instrument/afl-clang-fast++ . || true

.PHONY: clean help

help:
	@echo "all\nafl-2.57b\ninstrument\nclean\nhelp"

clean:
	$(MAKE) -C instrument clean
	$(MAKE) -C afl-2.57b clean
	rm -f afl-cmin afl-plot afl-whatsup Android.bp afl-analyze afl-as afl-clang afl-clang++ afl-fuzz afl-g++ afl-gcc afl-gotcpu afl-showmap afl-tmin as
	rm -f aflgo-clang aflgo-clang++ afl-clang-fast afl-clang-fast++ aflgo-pass.so aflgo-runtime.o aflgo-runtime-64.o aflgo-runtime-32.o