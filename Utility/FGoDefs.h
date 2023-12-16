/**
 *
 *
 *
 */

#ifndef FGODEFS_H_
#define FGODEFS_H_

// Environment variable name for project root directory
#define PROJ_ROOT_ENVAR "FGO_PROJ_ROOT_DIR"

// Environment variable name for distance directory
#define DIST_DIR_ENVAR "FGO_DIST_DIR"

// Environment variable name for usage of native clang
#define NATIVE_CLANG_ENVAR "FGO_NATIVE_CLANG"

// LLVM option name for distance directory
#define LLVM_OPT_DISTDIR_NAME "distdir"

// LLVM option name for project root directory
#define LLVM_OPT_PROJROOT_NAME "projroot"

// Hint for FGo compiler
#define COMPILER_HINT "FGo LLVM Pass"

// Name of final distance file
#define FINAL_DISTANCE_FILENAME "bb.distance.final"

// Name of depth-first distance file
#define DF_DISTANCE_FILENAME "bb.distance.df"

// Name of backtrace distance file
#define BT_DISTANCE_FILENAME "bb.distance.bt"

// Name of target information file for fuzzing
#define TARGET_INFO_FILENAME "target.info"

// FGo Parameter: a maximal count for target locations
#define FGO_TARGET_MAX_COUNT 64

// FGo Parameter: a constant distance for an external function call
#define FGO_EXTERNAL_CALL_DIST 50

// FGo Parameter: a constant distance for a recursive function call
#define FGO_RECURSIVE_CALL_DIST 45

// FGo Parameter: a constant distance for a block inner a function call
#define FGO_INNER_CALL_DIST 30

#endif