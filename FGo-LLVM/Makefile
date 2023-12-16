#
#
#

LLVM_PASS_LIB_NAME = llvm-pass
LLVM_RUNTIME_OBJ_NAME = llvm-runtime

## LLVM_DIR
ifndef LLVM_DIR
LLVM_DIR=../Dependency/llvm-14.0.0.obj
endif
ifeq ($(wildcard $(LLVM_DIR)),)
$(error "The 'LLVM_DIR'='$(LLVM_DIR)' doesn't exist")
else
LLVM_DIR := $(shell readlink -f $(LLVM_DIR))
LLVM_CONFIG = $(LLVM_DIR)/bin/llvm-config
ifeq ($(wildcard $(LLVM_CONFIG)),)
$(error "The executable '$(LLVM_CONFIG)' doesn't exist")
endif
CC = $(LLVM_DIR)/bin/clang
ifeq ($(wildcard $(CC)),)
$(error "The compiler '$(CC)' doesn't exist")
endif
CXX = $(LLVM_DIR)/bin/clang++
ifeq ($(wildcard $(CXX)),)
$(error "The compiler '$(CXX)' doesn't exist")
endif
endif

## JSONCPP_DIR
ifndef JSONCPP_DIR
JSONCPP_DIR=../Dependency/jsoncpp
endif
ifeq ($(wildcard $(JSONCPP_DIR)),)
$(error "The 'JSONCPP_DIR'='$(JSONCPP_DIR)' doesn't exist")
else
JSONCPP_HEADER = $(JSONCPP_DIR)/include
ifeq ($(wildcard $(JSONCPP_HEADER)),)
$(error "The jsoncpp header directory $(JSONCPP_HEADER) doesn't exist")
endif
JSONCPP_OBJ_PATH = $(JSONCPP_DIR)/Release-build/src/lib_json/CMakeFiles/jsoncpp_object.dir
ifeq ($(wildcard $(JSONCPP_OBJ_PATH)),)
$(error "The jsoncpp object directory $(JSONCPP_OBJ_PATH) doesn't exist")
endif
JSONCPP_OBJECTS = $(JSONCPP_OBJ_PATH)/json_reader.cpp.o $(JSONCPP_OBJ_PATH)/json_writer.cpp.o $(JSONCPP_OBJ_PATH)/json_value.cpp.o
endif

## INDICATORS_DIR
ifndef INDICATORS_DIR
INDICATORS_DIR=../Dependency/indicators
endif
ifeq ($(wildcard $(INDICATORS_DIR)),)
$(error "The 'INDICATORS_DIR'='$(INDICATORS_DIR)' doesn't exist")
else
INDICATORS_HEADER = $(INDICATORS_DIR)/include
ifeq ($(wildcard $(INDICATORS_HEADER)),)
$(error "The indicators header directory '$(INDICATORS_HEADER)' doesn't exist")
endif
endif

CFLAGS      ?= -O3 -funroll-loops
CFLAGS      += -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign

CXXFLAGS	?= -I$(INDICATORS_HEADER)

CL_CXXFLAGS  = -std=c++17 -g $(CXXFLAGS) -DLLVM_PASS_LIB_NAME=\""$(LLVM_PASS_LIB_NAME)"\" -DLLVM_RUNTIME_OBJ_NAME=\""$(LLVM_RUNTIME_OBJ_NAME)"\" -DCOMPILER_CLANG_PATH=\""$(CC)"\" -DCOMPILER_CLANGPP_PATH=\""$(CXX)"\"

PA_CXXFLAGS  = $(CXXFLAGS) -O3 -funroll-loops
PA_CXXFLAGS  += -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign
PA_CXXFLAGS  += -I$(JSONCPP_HEADER) $(shell $(LLVM_CONFIG) --cxxflags) -Wl,-znodelete -fno-rtti -fpic -Wno-variadic-macros

CL_LDFLAGS   = $(LDFLAGS)

PA_LDFLAGS   = $(LDFLAGS) -shared
ifeq "$(shell uname)" "Darwin"
	PA_LDFLAGS += -Wl,-flat_namespace -Wl,-undefined,suppress
endif
PA_LDFLAGS   += $(shell $(LLVM_CONFIG) --ldflags) $(JSONCPP_OBJECTS)

LLVM_PASS_LIB = $(LLVM_PASS_LIB_NAME).so
LLVM_RUNTIME_OBJ = $(LLVM_RUNTIME_OBJ_NAME).o
LLVM_RUNTIME_OBJ32 = $(LLVM_RUNTIME_OBJ_NAME).32.o
LLVM_RUNTIME_OBJ64 = $(LLVM_RUNTIME_OBJ_NAME).64.o
CLANG_WRAPPER = fgo-clang
CLANG_WRAPPER_CPP = fgo-clang++

FGO_DEFINITION_FILE = ../Utility/FGoDefs.h
NATIVE_CLANG_ENVAR = $(shell grep 'NATIVE_CLANG_ENVAR' $(FGO_DEFINITION_FILE) | awk '{print $$3}' | tr -d '"')
DIST_DIR_ENVAR = $(shell grep 'DIST_DIR_ENVAR' $(FGO_DEFINITION_FILE) | awk '{print $$3}' | tr -d '"')
PROJ_ROOT_ENVAR = $(shell grep 'PROJ_ROOT_ENVAR' $(FGO_DEFINITION_FILE) | awk '{print $$3}' | tr -d '"')
CLANG_WRAPPER_PATH = $(CURDIR)/$(CLANG_WRAPPER)
CLANG_WRAPPER_CPP_PATH = $(CURDIR)/$(CLANG_WRAPPER_CPP)
FGO_COMPILER_SETUP_NAIVE = fgo-compiler-setup.in
FGO_COMPILER_SETUP = fgo-compiler-setup

BUILD_TASKS = $(LLVM_PASS_LIB) $(CLANG_WRAPPER) $(LLVM_RUNTIME_OBJ) $(LLVM_RUNTIME_OBJ32) $(LLVM_RUNTIME_OBJ64) $(FGO_COMPILER_SETUP)

all: $(BUILD_TASKS)

$(LLVM_PASS_LIB): LLVM-Pass.cpp
	@$(CXX) $(PA_CXXFLAGS) $^ -o $@ $(PA_LDFLAGS)
	@echo "CXX => $@"

$(CLANG_WRAPPER): Clang-Wrapper.cpp
	@$(CXX) $(CL_CXXFLAGS) $^ -o $@ $(CL_LDFLAGS)
	@echo "CXX => $@"
	@ln -sf $(CLANG_WRAPPER) $(CLANG_WRAPPER_CPP)
	@echo "LN => $(CLANG_WRAPPER_CPP)"

$(LLVM_RUNTIME_OBJ): LLVM-Runtime.c
	@$(CC) $(CFLAGS) -fPIC -c $^ -o $@
	@echo "CC => $@"

$(LLVM_RUNTIME_OBJ32): LLVM-Runtime.c
	@printf "CC => $@ ..."
	@$(CC) $(CFLAGS) -m32 -fPIC -c $^ -o $@ 2> /dev/null; if [ "$$?" = "0" ]; then echo "success!"; else echo "failed (that's fine)"; fi

$(LLVM_RUNTIME_OBJ64): LLVM-Runtime.c
	@printf "CC => $@ ..."
	@$(CC) $(CFLAGS) -m64 -fPIC -c $^ -o $@ 2> /dev/null; if [ "$$?" = "0" ]; then echo "success!"; else echo "failed (that's fine)"; fi

test_build: $(BUILD_TASKS)
	@echo "Performing a simple test..."
	@unset AFL_USE_ASAN AFL_USE_MSAN AFL_INST_RATIO; AFL_QUIET=1; unset FGO_NATIVE_CLANG; FGO_PROJ_ROOT_DIR=`pwd`/test FGO_DIST_DIR=`pwd`/test ./$(CLANG_WRAPPER) -g test/instr.c -o test/instr &> /dev/null; DR="$$?"; rm -f test/instr; if [ "$$DR" = "0" ]; then echo; echo "Oops, the instrumentation failed!"; echo; exit 1; fi
	@echo "All right, the instrumentation works!"

$(FGO_COMPILER_SETUP) : $(FGO_COMPILER_SETUP_NAIVE)
	@cp -f $< $@
	@sed -i 's@CLANG_WRAPPER_CC@$(CLANG_WRAPPER_PATH)@g' $@
	@sed -i 's@CLANG_WRAPPER_CPP@$(CLANG_WRAPPER_CPP_PATH)@g' $@
	@sed -i 's@NATIVE_CLANG_ENVAR@$(NATIVE_CLANG_ENVAR)@g' $@
	@sed -i 's@DIST_DIR_ENVAR@$(DIST_DIR_ENVAR)@g' $@
	@sed -i 's@PROJ_ROOT_ENVAR@$(PROJ_ROOT_ENVAR)@g' $@
	@chmod +x $@
	@echo "GENERATE => $@"

.PHONY: clean

clean:
	@rm -f $(CLANG_WRAPPER) $(CLANG_WRAPPER_CPP) $(LLVM_RUNTIME_OBJ) $(LLVM_RUNTIME_OBJ32) $(LLVM_RUNTIME_OBJ64) $(LLVM_PASS_LIB) $(FGO_COMPILER_SETUP)