/**
 *
 *
 */

#ifndef FGOUTILS_HPP_
#define FGOUTILS_HPP_

#include "indicators/termcolor.hpp"

#include <iostream>
#include <string>

namespace FGo
{

    /// @brief Abort when the `_flag` is false and output `_msg` to standard error.
    /// @param _flag
    /// @param _msg
    inline void abortOnError(
        bool _flag, const std::string &_msg, const char *funcName, const char *fileName, int line)
    {
        if (!_flag)
        {
            std::cerr << termcolor::bold << termcolor::bright_red
                      << "[x] ABORT: " << termcolor::white << _msg << termcolor::reset << "\n";
            std::cerr << termcolor::bold << termcolor::white
                      << "     Location: " << termcolor::reset << funcName << "(), " << fileName
                      << ":" << line << termcolor::reset << "\n";
            std::cerr << std::endl;
            exit(1);
        }
    }

    inline void warnOnError(
        bool _flag, const std::string &_msg, const char *funcName, const char *fileName, int line)
    {
        if (!_flag)
        {
            std::cerr << termcolor::bold << termcolor::bright_yellow
                      << "[!] WARNING: " << termcolor::white << _msg << termcolor::reset << "\n";
            std::cerr << termcolor::bold << termcolor::white
                      << "     Location: " << termcolor::reset << funcName << "(), " << fileName
                      << ":" << line << termcolor::reset << "\n";
            std::cerr << std::endl;
        }
    }

    inline void highlightSome(const std::string &highlight, const std::string &msg)
    {
        std::cerr << termcolor::bold << termcolor::bright_cyan << highlight << termcolor::reset << " " << msg
                  << std::endl;
    }

    inline void succeedSome(const std::string &highlight, const std::string &msg)
    {
        std::cerr << termcolor::bold << termcolor::bright_green << highlight << termcolor::reset << " " << msg << std::endl;
    }

// inline void saySome(const std::string &highlight, const std::string &msg)
// {
//     std::cout << termcolor::bright_cyan << highlight << termcolor::reset << " " << msg
//               << std::endl;
// }

/// Abort when some errors occur. Output error information to standard error output with the
/// debug location defined by some [standard predefined
/// macros](https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html).
#define AbortOnError(flag, msg) abortOnError(flag, msg, __FUNCTION__, __FILE__, __LINE__)

/// Warn when some errors occur. Output error information to standard output with the debug
/// location defined by some [standard predefined
/// macros](https://gcc.gnu.org/onlinedocs/cpp/Standard-Predefined-Macros.html).
#define WarnOnError(flag, msg) warnOnError(flag, msg, __FUNCTION__, __FILE__, __LINE__)

/// Highlight some hints
#define HighlightSome(highlight, msg) highlightSome(highlight, msg)

/// Highlight some hints when succeeding in performing a task
#define SucceedSome(highlight, msg) succeedSome(highlight, msg)

    // #define SaySome(highlight, msg) saySome(highlight, msg)

#define PROJ_ROOT_ENVAR "FGO_PROJ_ROOT_DIR"
#define DIST_DIR_ENVAR "FGO_DIST_DIR"
#define LLVM_OPT_DISTDIR_NAME "distdir"
#define LLVM_OPT_PROJROOT_NAME "projroot"
#define COMPILER_HINT "FGo LLVM Pass"
#define FINAL_DISTANCE_FILENAME "bb.distance.final"

} // namespace FGo

#endif