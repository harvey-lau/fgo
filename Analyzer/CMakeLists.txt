cmake_minimum_required(VERSION 3.4.3)

project("FGo-Analyzer")

if (DEFINED LLVM_DIR)
    set(ENV{LLVM_DIR} "${LLVM_DIR}")
endif()

# find LLVM libraries
if (DEFINED ENV{LLVM_DIR})
    # We need to match the build environment for LLVM:
    # In particular, we need C++11 and the -fno-rtti flag
    set(CMAKE_CXX_STANDARD 14)
    
	if(CMAKE_BUILD_TYPE MATCHES "Debug")
		set(CMAKE_CXX_FLAGS "-fPIC -std=gnu++14 -O0 -fno-rtti -Wno-deprecated")
	else()
		set(CMAKE_CXX_FLAGS "-fPIC -std=gnu++14 -O3 -fno-rtti -Wno-deprecated")
	endif()

	set(CMAKE_C_FLAGS "-fPIC")

    find_package(LLVM REQUIRED CONFIG)
    
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(AddLLVM)

    add_definitions(${LLVM_DEFINITIONS})
    include_directories(${LLVM_INCLUDE_DIRS})

else()
    message(FATAL_ERROR "\
WARNING: The LLVM_DIR var was not set (required for an out-of-source build)!\n\
Please set this to environment variable to point to the LLVM build directory\
(e.g. on linux: export LLVM_DIR=/path/to/llvm/build/dir)")
endif()

# set SVF directory
if (EXISTS "${SVF_DIR}")
else()
    set(SVF_DIR $ENV{SVF_DIR})
    if(EXISTS "${SVF_DIR}")
    else()
    message(FATAL_ERROR "\
WARNING: The SVF_DIR var was not set (required for an out-of-source build)!\n\
Please set this to environment variable to point to the SVF_DIR directory or set this variable to cmake configuration\n
(e.g. on linux: export SVF_DIR=/path/to/SVF/dir) \n or \n \n(make the project via: cmake -DSVF_DIR=your_path_to_SVF) ")
    endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
    MESSAGE (STATUS "building SVF in debug mode")
    if (EXISTS "${SVF_DIR}/Debug-build")
        set(SVF_BIN "${SVF_DIR}/Debug-build")
    else()
        set(SVF_BIN "${SVF_DIR}/Release-build")
    endif()
else()
    MESSAGE (STATUS "building SVF in release mode")
    set(SVF_BIN "${SVF_DIR}/Release-build")
endif()
set(SVF_HEADER "${SVF_DIR}/svf/include")
set(SVF_LLVM_HEADER "${SVF_DIR}/svf-llvm/include")
set(SVF_LIB "${SVF_BIN}/svf-llvm/libSvfLLVM.a" "${SVF_BIN}/svf/libSvfCore.a")
set(SVF_BIN_HEADER "${SVF_BIN}/include")
include_directories(${SVF_HEADER}
                    ${SVF_LLVM_HEADER}
                    ${SVF_BIN_HEADER})

# set z3 env
if (DEFINED Z3_DIR)
    set(ENV{Z3_DIR} "${Z3_DIR}")
endif()
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if(EXISTS "${Z3_DIR}/src")
        find_package(Z3 REQUIRED CONFIG)
        include_directories(${Z3_CXX_INCLUDE_DIRS})
    else()
        find_library(Z3_LIBRARIES NAMES libz3.a libz3.so
                HINTS $ENV{Z3_DIR}
                PATH_SUFFIXES bin)
        find_path(Z3_INCLUDES NAMES z3++.h
                HINTS $ENV{Z3_DIR}
                PATH_SUFFIXES include z3)
        if(NOT Z3_LIBRARIES OR NOT Z3_INCLUDES)
            message(FATAL_ERROR "Z3 not found!")
        endif()
        include_directories(${Z3_INCLUDES})
        LINK_DIRECTORIES(${Z3_DIR}/bin)
    endif()
else()
    find_library(Z3_LIBRARIES NAMES libz3.a libz3.so
            HINTS $ENV{Z3_DIR}
            PATH_SUFFIXES bin)
    find_path(Z3_INCLUDES NAMES z3++.h
            HINTS $ENV{Z3_DIR}
            PATH_SUFFIXES include z3)
    if(NOT Z3_LIBRARIES OR NOT Z3_INCLUDES)
        message(FATAL_ERROR "Z3 not found!")
    endif()
    include_directories(${Z3_INCLUDES})
    LINK_DIRECTORIES(${Z3_DIR}/bin)
endif()

# set jsoncpp env
if (DEFINED JSONCPP_DIR)
    set(ENV{JSONCPP_DIR} "${JSONCPP_DIR}")
endif()
if (EXISTS "${JSONCPP_DIR}")
else()
    set(JSONCPP_DIR $ENV{JSONCPP_DIR})
    if(EXISTS "${JSONCPP_DIR}")
    else()
    message(FATAL_ERROR "\
    WARNING: The JSONCPP_DIR var was not set (required for an out-of-source build)!\n\
    Please set this to environment variable to point to the JSONCPP_DIR directory or set this variable to cmake configuration\n
    (e.g. on linux: export JSONCPP_DIR=/path/to/jsoncpp/dir) \n or \n \n(make the project via: cmake -DJSONCPP_DIR=your_path_to_jsoncpp) ")
    endif()
endif()
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    if (EXISTS "${JSONCPP_DIR}/Debug-build")
        set(JSONCPP_BIN "${JSONCPP_DIR}/Debug-build")
    else()
        message(FATAL_ERROR "\
        WARNING: Debug-build directory was not found under ${JSONCPP_DIR}
        ")
    endif()
else()
    if (EXISTS "${JSONCPP_DIR}/Release-build")
        set(JSONCPP_BIN "${JSONCPP_DIR}/Release-build")
    else()
        message(FATAL_ERROR "\
        WARNING: Release-build directory was not found under ${JSONCPP_DIR}
        ")
    endif()
endif()
set(JSONCPP_HEADER "${JSONCPP_DIR}/include")
set(JSONCPP_LIB_DIR "${JSONCPP_BIN}/lib")
set(JSONCPP_LIB "${JSONCPP_LIB_DIR}/libjsoncpp.a")
include_directories(${JSONCPP_HEADER})
LINK_DIRECTORIES(${JSONCPP_LIB_DIR})

# set indicators env
if (DEFINED INDICATORS_DIR)
    set(ENV{INDICATORS_DIR} "${INDICATORS_DIR}")
endif()
if (EXISTS "${INDICATORS_DIR}")
else()
    set(INDICATORS_DIR $ENV{INDICATORS_DIR})
    if(EXISTS "${INDICATORS_DIR}")
    else()
    message(FATAL_ERROR "\
    WARNING: The INDICATORS_DIR var was not set (required for an out-of-source build)!\n\
    Please set this to environment variable to point to the INDICATORS_DIR directory or set this variable to cmake configuration\n
    (e.g. on linux: export INDICATORS_DIR=/path/to/indicators/dir) \n or \n \n(make the project via: cmake -DINDICATORS_DIR=your_path_to_indicators) ")
    endif()
endif()
set(INDICATORS_HEADER "${INDICATORS_DIR}/include")
include_directories(${INDICATORS_HEADER})

# add other headers
set(UTILITY_DIR "${PROJECT_SOURCE_DIR}/../Utility")
if (EXISTS "${UTILITY_DIR}")
else()
    message(FATAL_ERROR "\
    WARNING: The directory 'Utility' was not found under the project root directory '${PROJECT_SOURCE_DIR}/../' !\n\
    You should check whether this directory was destroyed.")
endif()
include_directories(${UTILITY_DIR})


add_subdirectory(src)

add_custom_target(extapi_ir ALL
    COMMAND ${CMAKE_COMMAND} -E copy ${SVF_BIN}/svf-llvm/extapi.bc ${PROJECT_BINARY_DIR}/bin
    DEPENDS ${SVF_BIN}/svf-llvm/extapi.bc
    COMMENT "Copying ${SVF_BIN}/svf-llvm/extapi.bc to ${PROJECT_BINARY_DIR}/bin"
)