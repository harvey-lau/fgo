#
#
#

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

PARSER_LIBRARY_NAME := fgo-fuzzing-utils.so
HELPER_OBJECT_NAME := fgo-fuzzing-helper.o
PARSER_LIBRARY_PATH := $(shell readlink -f $(PARSER_LIBRARY_NAME))

CXXFLAGS ?= -I$(JSONCPP_HEADER)
CXXFLAGS += -std=c++17 -g -fpic -O3 -funroll-loops -D_FORTIFY_SOURCE=2
LDFLAGS  ?= $(JSONCPP_OBJECTS)

CFLAGS ?= -O3 -funroll-loops -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign
CFLAGS += -DPARSER_LIBRARY_PATH=\""$(PARSER_LIBRARY_PATH)"\" 

FGO_PARSER_LIB = $(PARSER_LIBRARY_NAME)
FGO_HELPER_OBJ = $(HELPER_OBJECT_NAME)

all : $(FGO_PARSER_LIB) $(FGO_HELPER_OBJ)

$(FGO_PARSER_LIB) : FGoFuzzingUtils.cpp
	@$(CXX) $(CXXFLAGS) -shared $< -o $@ $(LDFLAGS)
	@echo "CXX => $@"

$(FGO_HELPER_OBJ) : FGoFuzzingHelper.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC => $@"

.PHONY: clean clean_all

clean:
	@rm -f $(FGO_HELPER_OBJ)

clean_all:
	@rm -f $(FGO_HELPER_OBJ) $(FGO_PARSER_LIB)