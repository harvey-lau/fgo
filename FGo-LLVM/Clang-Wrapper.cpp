/*
   aflgo compiler
   --------------

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

 */

/**
 * FGo compiler wrapper
 *
 */

#define AFL_MAIN
#define AFL_LLVM_PASS

#include "../AFL-Fuzz/config.h"
#include "../AFL-Fuzz/types.h"
#include "../Utility/FGoDefs.h"
#include "../Utility/FGoUtils.hpp"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>
#include <vector>

#ifndef LLVM_PASS_LIB_NAME
    #define LLVM_PASS_LIB_NAME "llvm-pass"
#endif
#ifndef LLVM_RUNTIME_OBJ_NAME
    #define LLVM_RUNTIME_OBJ_NAME "llvm-runtime"
#endif
#ifndef COMPILER_CLANG_PATH
    #define COMPILER_CLANG_PATH "clang"
#endif
#ifndef COMPILER_CLANGPP_PATH
    #define COMPILER_CLANGPP_PATH "clang++"
#endif

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

namespace FGo
{

class CompilerWrapper
{
private:
    std::vector<std::string> m_arguments;
    bool m_isCpp;
    bool m_isInstrument;
    bool m_isNative;

    std::string m_LLVMPassLib;
    std::string m_LLVMRuntimeObj;
    std::string m_LLVMRuntime32Obj;
    std::string m_LLVMRuntime64Obj;

public:
    CompilerWrapper(int argc, char **argv);
    ~CompilerWrapper() = default;

    /// @brief Update the arguments
    void updateArguments();

    void execute();
};

static std::string joinPath(const std::string &basePath, const std::string &fileName)
{
    std::filesystem::path fsBasePath = basePath;
    std::filesystem::path fsFileName = fileName;
    std::filesystem::path fsResult = fsBasePath / fsFileName;
    return fsResult.string();
}

static std::string parentPath(const std::string &filePath)
{
    std::filesystem::path fsFilePath = filePath;
    return fsFilePath.parent_path().string();
}

static bool pathExists(const std::string &filePath)
{
    std::filesystem::path fsFilePath = filePath;
    return std::filesystem::exists(fsFilePath);
}

static bool pathIsDirectory(const std::string &filePath)
{
    std::filesystem::path fsFilePath = filePath;
    return std::filesystem::is_directory(fsFilePath);
}

static bool pathIsFile(const std::string &filePath)
{
    std::filesystem::path fsFilePath = filePath;
    return std::filesystem::is_regular_file(fsFilePath);
}

static std::string getExeDirPath()
{
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        std::string executablePath(buffer);
        return parentPath(executablePath);
    }
    else {
        return "";
    }
}

CompilerWrapper::CompilerWrapper(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "This is a compiler wrapper for FGo.\n\n"
                  << "The following arguments supported by AFL are still supported by this "
                     "compiler.\n"
                  << "'AFL_CXX' or 'AFL_CC'\n"
                  << "'AFL_HARDEN'\n"
                  << "'AFL_USE_ASAN' or 'AFL_USE_MSAN'\n"
                  << "'AFL_DONT_OPTIMIZE'\n"
                  << "'AFL_NO_BUILTIN'\n"
                  << "'AFL_QUIET'\n"
                  << std::endl;
        exit(1);
    }

    std::string exeFile = argv[0];
    if (exeFile.size() > 2 && exeFile.compare(exeFile.size() - 2, exeFile.size(), "++") == 0)
        m_isCpp = true;
    else m_isCpp = false;

    std::string exeDir = getExeDirPath();
    AbortOnError(!exeDir.empty(), "Failed to get the directory of the executable " + exeFile);

    std::string passLibFileName = std::string(LLVM_PASS_LIB_NAME) + ".so";
    m_LLVMPassLib = joinPath(exeDir, passLibFileName);
    AbortOnError(
        !m_LLVMPassLib.empty(), std::string("Failed get the path of ") + passLibFileName
    );
    AbortOnError(
        pathExists(m_LLVMPassLib) && pathIsFile(m_LLVMPassLib),
        std::string("Failed find '") + passLibFileName + "' under " + exeDir
    );

    std::string runtimeObjFileName = std::string(LLVM_RUNTIME_OBJ_NAME) + ".o";
    m_LLVMRuntimeObj = joinPath(exeDir, runtimeObjFileName);
    AbortOnError(
        !m_LLVMRuntimeObj.empty(), std::string("Failed get the path of ") + runtimeObjFileName
    );
    AbortOnError(
        pathExists(m_LLVMRuntimeObj) && pathIsFile(m_LLVMRuntimeObj),
        std::string("Failed find '") + runtimeObjFileName + "' under " + exeDir
    );

    m_LLVMRuntime32Obj = joinPath(exeDir, std::string(LLVM_RUNTIME_OBJ_NAME) + ".32.o");
    m_LLVMRuntime64Obj = joinPath(exeDir, std::string(LLVM_RUNTIME_OBJ_NAME) + ".64.o");

    m_arguments.resize(argc - 1);
    m_isInstrument = false;
    std::string optionDistDir = std::string("-") + LLVM_OPT_DISTDIR_NAME;
    for (int i = 1; i < argc; ++i) {
        m_arguments[i - 1] = argv[i];
        if (m_arguments[i - 1].compare(0, optionDistDir.size(), optionDistDir) == 0)
            m_isInstrument = true;
    }
    if (getenv(DIST_DIR_ENVAR)) m_isInstrument = true;
    if (getenv(NATIVE_CLANG_ENVAR)) m_isNative = true;
}

void CompilerWrapper::updateArguments()
{
    std::vector<std::string> newArgs;
    bool maybeLinking = true, isXSet = false, isASanSet = false, isFortifySet = false;
    unsigned bitMode = 0;
    std::string optionDistDir = std::string("-") + LLVM_OPT_DISTDIR_NAME;
    std::string optionProjRoot = std::string("-") + LLVM_OPT_PROJROOT_NAME;

    // There are two ways to compile afl-clang-fast. In the traditional mode, we
    // use afl-llvm-pass.so to inject instrumentation. In the experimental
    // 'trace-pc-guard' mode, we use native LLVM instrumentation callbacks
    // instead. The latter is a very recent addition - see:
    // http://clang.llvm.org/docs/SanitizerCoverage.html#tracing-pcs-with-guards

#ifdef USE_TRACE_PC
    newArgs.push_back("-fsanitize-coverage=trace-pc-guard");
    newArgs.push_back("-mllvm");
    newArgs.push_back("-sanitizer-coverage-block-threshold=0");
    #error AFLGO has not supported trace-pc-guard yet
#else
    newArgs.push_back("-fexperimental-new-pass-manager");
    newArgs.push_back("-fpass-plugin=" + m_LLVMPassLib);
#endif /* ^USE_TRACE_PC */

    newArgs.push_back("-Qunused-arguments");

    if (m_arguments.size() == 1 && m_arguments.front() == "-v") {
        maybeLinking = false;
    }

    for (const auto &curArg : m_arguments) {
        if (curArg.compare(0, optionDistDir.size(), optionDistDir) == 0 ||
            curArg.compare(0, optionProjRoot.size(), optionProjRoot) == 0)
            newArgs.push_back("-mllvm");

        if (curArg == "-m32") bitMode = 32;
        if (curArg == "-m64") bitMode = 64;

        if (curArg == "-x") isXSet = true;

        if (curArg == "-c" || curArg == "-S" || curArg == "-E") maybeLinking = false;

        if (curArg.compare(0, 11, "-fsanitize=") == 0) {
            if (curArg.find("address") != std::string::npos ||
                curArg.find("memory") != std::string::npos)
                isASanSet = true;
        }

        if (curArg.find("FORTIFY_SOURCE") != std::string::npos) isFortifySet = true;

        if (curArg == "-shared") maybeLinking = false;

        if (curArg == "-Wl,-z,defs" || curArg == "-Wl,--no-undefined") continue;

        newArgs.push_back(curArg);
    }

    // Env 'AFL_HARDEN'
    if (getenv("AFL_HARDEN")) {
        newArgs.push_back("-fstack-protector-all");
        if (!isFortifySet) newArgs.push_back("-D_FORTIFY_SOURCE=2");
    }

    // ASan and MSan
    if (!isASanSet) {
        if (getenv("AFL_USE_ASAN")) {

            if (getenv("AFL_USE_MSAN"))
                AbortOnError(false, "ASan and MSan are mutually exclusive");

            if (getenv("AFL_HARDEN"))
                AbortOnError(false, "ASan and AFL_HARDEN are mutually exclusive");

            newArgs.push_back("-U_FORTIFY_SOURCE");
            newArgs.push_back("-fsanitize=address");
        }
        else if (getenv("AFL_USE_MSAN")) {

            if (getenv("AFL_USE_ASAN"))
                AbortOnError(false, "MSan and ASan are mutually exclusive");

            if (getenv("AFL_HARDEN"))
                AbortOnError(false, "MSan and AFL_HARDEN are mutually exclusive");

            newArgs.push_back("-U_FORTIFY_SOURCE");
            newArgs.push_back("-fsanitize=memory");
        }
    }

    // Don't optimize
    if (!getenv("AFL_DONT_OPTIMIZE")) {
        newArgs.push_back("-g");
        // newArgs.push_back("-O3");
        newArgs.push_back("-funroll-loops");
    }

    // No builtin
    if (getenv("AFL_NO_BUILTIN")) {
        newArgs.push_back("-fno-builtin-strcmp");
        newArgs.push_back("-fno-builtin-strncmp");
        newArgs.push_back("-fno-builtin-strcasecmp");
        newArgs.push_back("-fno-builtin-strncasecmp");
        newArgs.push_back("-fno-builtin-memcmp");
    }

    newArgs.push_back("-D__AFL_HAVE_MANUAL_CONTROL=1");
    newArgs.push_back("-D__AFL_COMPILER=1");
    newArgs.push_back("-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1");

    // When the user tries to use persistent or deferred forkserver modes by
    // appending a single line to the program, we want to reliably inject a
    // signature into the binary (to be picked up by afl-fuzz) and we want
    // to call a function from the runtime .o file. This is unnecessarily
    // painful for three reasons:

    // 1) We need to convince the compiler not to optimize out the signature.
    //     This is done with __attribute__((used)).

    // 2) We need to convince the linker, when called with -Wl,--gc-sections,
    //     not to do the same. This is done by forcing an assignment to a
    //     'volatile' pointer.

    // 3) We need to declare __afl_persistent_loop() in the global namespace,
    //     but doing this within a method in a class is hard - :: and extern "C"
    //     are forbidden and __attribute__((alias(...))) doesn't work. Hence the
    //     __asm__ aliasing trick.

    newArgs.push_back("-D__AFL_LOOP(_A)="
                      "({ static volatile char *_B __attribute__((used)); "
                      " _B = (char*)\"" PERSIST_SIG "\"; "
#ifdef __APPLE__
                      "__attribute__((visibility(\"default\"))) "
                      "int _L(unsigned int) __asm__(\"___afl_persistent_loop\"); "
#else
                      "__attribute__((visibility(\"default\"))) "
                      "int _L(unsigned int) __asm__(\"__afl_persistent_loop\"); "
#endif /* ^__APPLE__ */
                      "_L(_A); })");

    newArgs.push_back("-D__AFL_INIT()="
                      "do { static volatile char *_A __attribute__((used)); "
                      " _A = (char*)\"" DEFER_SIG "\"; "
#ifdef __APPLE__
                      "__attribute__((visibility(\"default\"))) "
                      "void _I(void) __asm__(\"___afl_manual_init\"); "
#else
                      "__attribute__((visibility(\"default\"))) "
                      "void _I(void) __asm__(\"__afl_manual_init\"); "
#endif /* ^__APPLE__ */
                      "_I(); } while (0)");

    if (maybeLinking) {

        if (isXSet) {
            newArgs.push_back("-x");
            newArgs.push_back("none");
        }

        switch (bitMode) {

        case 0:
            newArgs.push_back(m_LLVMRuntimeObj);
            break;

        case 32:
            AbortOnError(
                pathExists(m_LLVMRuntime32Obj) && pathIsFile(m_LLVMRuntime32Obj),
                "'-m32' is not supported by this compiler wrapper"
            );
            newArgs.push_back(m_LLVMRuntime32Obj);
            break;

        case 64:
            AbortOnError(
                pathExists(m_LLVMRuntime64Obj) && pathIsFile(m_LLVMRuntime64Obj),
                "'-m64' is not supported by this compiler wrapper"
            );
            newArgs.push_back(m_LLVMRuntime64Obj);
            break;
        }
    }

    m_arguments = newArgs;
}

void CompilerWrapper::execute()
{
    std::string compilerName;
    // clang/clang++
    if (m_isCpp) {
        char *clangFromEnv = getenv("AFL_CXX");
        if (clangFromEnv) compilerName = clangFromEnv;
        else compilerName = COMPILER_CLANGPP_PATH;
    }
    else {
        char *clangFromEnv = getenv("AFL_CC");
        if (clangFromEnv) compilerName = clangFromEnv;
        else compilerName = COMPILER_CLANG_PATH;
    }

    char **cStyleArgs = new char *[m_arguments.size() + 2];
    cStyleArgs[0] = new char[compilerName.size() + 1];
    memset(cStyleArgs[0], 0, compilerName.size() + 1);
    strncpy(cStyleArgs[0], compilerName.c_str(), compilerName.size());
    for (size_t i = 1; i < m_arguments.size() + 1; ++i) {
        cStyleArgs[i] = new char[m_arguments[i - 1].size() + 1];
        memset(cStyleArgs[i], 0, m_arguments[i - 1].size() + 1);
        strncpy(cStyleArgs[i], m_arguments[i - 1].c_str(), m_arguments[i - 1].size());
    }
    cStyleArgs[m_arguments.size() + 1] = NULL;

    // // Debug test
    // std::cout << "Compiler: " << compilerName << std::endl;

    // // Debug test
    // for (const auto &arg : m_arguments) {
    //     std::cout << arg << std::endl;
    // }
    // for (size_t i = 0; i < m_arguments.size(); ++i) {
    //     std::cout << cStyleArgs[i] << std::endl;
    // }

    execvp(compilerName.c_str(), cStyleArgs);

    AbortOnError(false, "Failed to execute " + compilerName);
}
} // namespace FGo

/* Main entry point */

int main(int argc, char **argv)
{
    if (isatty(2) && !getenv("AFL_QUIET")) {

#ifdef USE_TRACE_PC
        FGo::HighlightSome("FGo Compiler (tpcg)", "");
#else
        FGo::HighlightSome("FGo Compiler", "");
#endif /* ^USE_TRACE_PC */
    }

    FGo::CompilerWrapper compiler(argc, argv);

    compiler.updateArguments();

    compiler.execute();

    return 0;
}
