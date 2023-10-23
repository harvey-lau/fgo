/*
  Copyright 2015 Google LLC All rights reserved.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

/*
   american fuzzy lop - LLVM-mode wrapper for clang
   ------------------------------------------------

   Written by Laszlo Szekeres <lszekeres@google.com> and
              Michal Zalewski <lcamtuf@google.com>

   LLVM integration design comes from Laszlo Szekeres.

   This program is a drop-in replacement for clang, similar in most respects
   to ../afl-gcc. It tries to figure out compilation mode, adds a bunch
   of flags, and then calls the real compiler.
*/

/*
    AFLGo: Directed Greybox Fuzzing
    -----------------------------------------------

    Written by AFLGo authors from https://github.com/aflgo/aflgo
*/

/*
    A Rust edition of afl-clang-fast for FGo
    -----------------------------------------------

    Written by Joshua Yao <joshuayao13@gmail.com>
            from https://github.com/L-Joshua-Y/aflgo#addition
                 https://github.com/harvey-lau/fgo#addition
*/

use std::collections::HashMap;
use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::process::{self, Stdio};

const VERSION_SERIAL: &'static str = "2.52b-rust";

const MACRO_PERSIST_SIG: &'static str = "##SIG_AFL_PERSISTENT##";
const MACRO_DEFER_SIG: &'static str = "##SIG_AFL_DEFER_FORKSRV##";

const FILE_LLVM_PASS_KEY: &'static str = "afl-llvm-pass";
const FILE_LLVM_RT_KEY: &'static str = "afl-llvm-rt";
const FILE_LLVM_RT32_KEY: &'static str = "afl-llvm-rt32";
const FILE_LLVM_RT64_KEY: &'static str = "afl-llvm-rt64";
const FILE_LLVM_PASS_NAME: &'static str = "afl-llvm-pass.so";
const FILE_LLVM_RT_NAME: &'static str = "afl-llvm-rt.o";
const FILE_LLVM_RT32_NAME: &'static str = "afl-llvm-rt-32.o";
const FILE_LLVM_RT64_NAME: &'static str = "afl-llvm-rt-64.o";

const FILE_AFL_CC: &'static str = "afl-clang-fast";
const FILE_AFL_CXX: &'static str = "afl-clang-fast++";
const FILE_BIN_CC: &'static str = "clang";
const FILE_BIN_CXX: &'static str = "clang++";

const ENV_AFL_CC: &'static str = "AFL_CC";
const ENV_AFL_CXX: &'static str = "AFL_CXX";

const ENV_AFL_PATH: &'static str = "AFL_PATH";
const ENV_AFLGO_PARAMS_CHECK: &'static str = "AFLGO_PARAMS_CHECK";
const ENV_AFLGO_DISTANCE_FILE: &'static str = "AFLGO_DISTANCE_FILE";
const ENV_AFLGO_TARGETS_FILE: &'static str = "AFLGO_TARGETS_FILE";
const ENV_AFLGO_OUTDIR: &'static str = "AFLGO_OUTDIR";
const ENV_AFLGO_ADDITIONAL_PARAMS: &'static str = "AFLGO_ADDITIONAL_PARAMS";

const ARG_DISTANCE_PROMPT: &'static str = "-distance";
const ARG_TARGETS_PROMPT: &'static str = "-targets";
const ARG_OUTDIR_PROMPT: &'static str = "-outdir";
const ARG_DISTANCE_PROMPT_LEN: usize = ARG_DISTANCE_PROMPT.len();
const ARG_TARGETS_PROMPT_LEN: usize = ARG_TARGETS_PROMPT.len();
const ARG_OUTDIR_PROMPT_LEN: usize = ARG_OUTDIR_PROMPT.len();

const ARG_SANITIZER_PROMPT: &'static str = "-fsanitize=";
const ARG_SANITIZER_PROMPT_LEN: usize = ARG_SANITIZER_PROMPT.len();

const ENV_AFL_QUIET: &'static str = "AFL_QUIET";
const ENV_AFL_HARDEN: &'static str = "AFL_HARDEN";
const ENV_AFL_INST_RATIO: &'static str = "AFL_INST_RATIO";
const ENV_AFL_NOT_OPTIMIZE: &'static str = "AFL_DONT_OPTIMIZE";
const ENV_AFL_NO_BUILTIN: &'static str = "AFL_NO_BUILTIN";
const ENV_AFL_USE_ASAN: &'static str = "AFL_USE_ASAN";
const ENV_AFL_USE_MSAN: &'static str = "AFL_USE_MSAN";
const ENV_AFL_USE_UBSAN: &'static str = "AFL_USE_UBSAN";

/// Leave G1 drawing mode
const COLOR_STOP_G1: &'static str = "\x0f";
/// Reset G1 to ASCII
const COLOR_RESET_G1: &'static str = "\x1b)B";
/// Show cursor
const COLOR_SHOW_CURSOR: &'static str = "\x1b[?25h";
/// Reset font color
const COLOR_RESET: &'static str = "\x1b[0m";
/// Light Red
const COLOR_LIGHT_RED: &'static str = "\x1b[1;91m";
/// Light Yellow
const COLOR_LIGHT_YELLOW: &'static str = "\x1b[1;93m";
/// Cyan
const COLOR_CYAN: &'static str = "\x1b[0;36m";
/// Bold
const COLOR_BOLD: &'static str = "\x1b[1;97m";

fn fatal<T: std::fmt::Display>(msg: T, location: &std::panic::Location<'_>) {
    #[cfg(feature = "MESSAGES_TO_STDOUT")]
    {
        println!(
            "{}{}{}{}{}\n[-] PROGRAM ABORT : {}{}",
            COLOR_STOP_G1,
            COLOR_RESET_G1,
            COLOR_SHOW_CURSOR,
            COLOR_RESET,
            COLOR_LIGHT_RED,
            COLOR_BOLD,
            msg
        );
        println!(
            "{}\n         Location : {}{}:{}\n",
            COLOR_LIGHT_RED,
            COLOR_RESET,
            location.file(),
            location.line()
        );
    }
    #[cfg(not(feature = "MESSAGES_TO_STDOUT"))]
    {
        eprintln!(
            "{}{}{}{}{}\n[-] PROGRAM ABORT : {}{}",
            COLOR_STOP_G1,
            COLOR_RESET_G1,
            COLOR_SHOW_CURSOR,
            COLOR_RESET,
            COLOR_LIGHT_RED,
            COLOR_BOLD,
            msg
        );
        eprintln!(
            "{}\n         Location : {}{}:{}\n",
            COLOR_LIGHT_RED,
            COLOR_RESET,
            location.file(),
            location.line()
        );
    }
    process::exit(1);
}

fn warnf<T: std::fmt::Display>(msg: T) {
    #[cfg(feature = "MESSAGES_TO_STDOUT")]
    {
        println!(
            "{}[!] {}WARNING: {}{}\n",
            COLOR_LIGHT_YELLOW, COLOR_BOLD, COLOR_RESET, msg
        );
    }
    #[cfg(not(feature = "MESSAGES_TO_STDOUT"))]
    {
        eprintln!(
            "{}[!] {}WARNING: {}{}\n",
            COLOR_LIGHT_YELLOW, COLOR_BOLD, COLOR_RESET, msg
        );
    }
}

fn outln<T: std::fmt::Display>(msg: T) {
    #[cfg(feature = "MESSAGES_TO_STDOUT")]
    {
        println!("{}", msg);
    }
    #[cfg(not(feature = "MESSAGES_TO_STDOUT"))]
    {
        eprintln!("{}", msg);
    }
}

fn title() {
    #[cfg(all(feature = "USE_TRACE_PC", feature = "MESSAGES_TO_STDOUT"))]
    {
        println!(
            "{}fgo-compiler (yeah!) [tpcg] {}{}{}",
            COLOR_CYAN, COLOR_BOLD, VERSION_SERIAL, COLOR_RESET
        );
    }
    #[cfg(all(not(feature = "USE_TRACE_PC"), feature = "MESSAGES_TO_STDOUT"))]
    {
        println!(
            "{}fgo-compiler (yeah!) {}{}{}",
            COLOR_CYAN, COLOR_BOLD, VERSION_SERIAL, COLOR_RESET
        );
    }
    #[cfg(all(feature = "USE_TRACE_PC", not(feature = "MESSAGES_TO_STDOUT")))]
    {
        eprintln!(
            "{}fgo-compiler (yeah!) [tpcg] {}{}{}",
            COLOR_CYAN, COLOR_BOLD, VERSION_SERIAL, COLOR_RESET
        );
    }
    #[cfg(all(not(feature = "USE_TRACE_PC"), not(feature = "MESSAGES_TO_STDOUT")))]
    {
        eprintln!(
            "{}fgo-compiler (yeah!) {}{}{}",
            COLOR_CYAN, COLOR_BOLD, VERSION_SERIAL, COLOR_RESET
        );
    }
}

fn cmp_first_chars(str1: &str, str2: &str, len: usize) -> bool {
    if let (Some(substr1), Some(substr2)) = (str1.get(0..len), str2.get(0..len)) {
        if substr1.eq(substr2) {
            return true;
        } else {
            return false;
        }
    } else {
        return false;
    }
}

fn cmp_with_group_strings(str1: &str, strings: &Vec<String>) -> bool {
    for str2 in strings {
        if str1.eq(str2) {
            return true;
        }
    }
    return false;
}

/// Finds objects under the directory where the compiler afl-clang-fast locates
/// or get the AFL_PATH from the environment variable.
///
/// If the function fails to find the LLVM pass library "afl-llvm-pass.so" or LLVM
/// runtime object "afl-llvm-rt.o", it will abort the program.
///
/// The parameter `program` is the first parameter of this program from the command line.
/// The file paths found by this function will be stored in the hash map `afl_config_files`.
fn find_objs(parent_dir: &Path, afl_config_files: &mut HashMap<&str, String>) {
    // The directory where the program binary locates
    let llvm_pass_filepath = parent_dir.join(FILE_LLVM_PASS_NAME);
    let llvm_rt_filepath = parent_dir.join(FILE_LLVM_RT_NAME);
    if llvm_pass_filepath.exists()
        && llvm_rt_filepath.exists()
        && llvm_pass_filepath.is_file()
        && llvm_rt_filepath.is_file()
    {
        afl_config_files.insert(
            FILE_LLVM_PASS_KEY,
            llvm_pass_filepath.to_string_lossy().to_string(),
        );
        afl_config_files.insert(
            FILE_LLVM_RT_KEY,
            llvm_rt_filepath.to_string_lossy().to_string(),
        );
        afl_config_files.insert(
            FILE_LLVM_RT32_KEY,
            parent_dir
                .join(FILE_LLVM_RT32_NAME)
                .to_string_lossy()
                .to_string(),
        );
        afl_config_files.insert(
            FILE_LLVM_RT64_KEY,
            parent_dir
                .join(FILE_LLVM_RT64_NAME)
                .to_string_lossy()
                .to_string(),
        );
    } else {
        // The directory from the environment variable
        if let Ok(env_val) = env::var(ENV_AFL_PATH) {
            let afl_path_dir = Path::new(&env_val);
            if !afl_path_dir.exists() {
                fatal(
                    format!(
                        "Unable to locate '{}'(={}). It doesn't exist.",
                        ENV_AFL_PATH, env_val
                    ),
                    std::panic::Location::caller(),
                );
            }
            if !afl_path_dir.is_dir() {
                fatal(
                    format!(
                        "Unable to locate '{}'(={}). It's not a directory.",
                        ENV_AFL_PATH, env_val
                    ),
                    std::panic::Location::caller(),
                );
            }
            let new_llvm_pass_filepath = afl_path_dir.join(FILE_LLVM_PASS_NAME);
            let new_llvm_rt_filepath = afl_path_dir.join(FILE_LLVM_RT_NAME);
            if new_llvm_pass_filepath.exists()
                && new_llvm_rt_filepath.exists()
                && new_llvm_pass_filepath.is_file()
                && new_llvm_rt_filepath.is_file()
            {
                afl_config_files.insert(
                    FILE_LLVM_PASS_KEY,
                    new_llvm_pass_filepath.to_string_lossy().to_string(),
                );
                afl_config_files.insert(
                    FILE_LLVM_RT_KEY,
                    new_llvm_rt_filepath.to_string_lossy().to_string(),
                );
                afl_config_files.insert(
                    FILE_LLVM_RT32_KEY,
                    afl_path_dir
                        .join(FILE_LLVM_RT32_NAME)
                        .to_string_lossy()
                        .to_string(),
                );
                afl_config_files.insert(
                    FILE_LLVM_RT64_KEY,
                    afl_path_dir
                        .join(FILE_LLVM_RT64_NAME)
                        .to_string_lossy()
                        .to_string(),
                );
            } else {
                fatal(
                    format!(
                        "Unable to find {} or {} under '{}'(={}).",
                        FILE_LLVM_RT_NAME, FILE_LLVM_PASS_NAME, ENV_AFL_PATH, env_val
                    ),
                    std::panic::Location::caller(),
                );
            }
        } else {
            fatal(
                format!(
                    "Unable to find {} or {}. Please set '{}'.",
                    FILE_LLVM_RT_NAME, FILE_LLVM_PASS_NAME, ENV_AFL_PATH
                ),
                std::panic::Location::caller(),
            );
        }
    }
}

/// Checks and edits program parameters from command line.
/// It first get the AFLGo specific parameters and the additional
/// parameters from the environment variables.
/// And then this function checks these parameters and remove duplicates.
/// Besides, it will add or edit some parameter arguments according to
/// system configuration and envrionment variables.
///
/// New parameter arguments are transfered to `new_args` including the
/// program binary.
fn check_edit_params(
    args: &Vec<String>,
    new_args: &mut Vec<String>,
    afl_config_files: &HashMap<&str, String>,
) {
    // ENV_AFLGO_PARAMS_CHECK
    let mut check_flag = false;
    if let Ok(_) = env::var(ENV_AFLGO_PARAMS_CHECK) {
        check_flag = true;
    }

    new_args.clear();

    // proragm executable
    if args[0].ends_with(FILE_AFL_CXX) {
        if let Ok(afl_cxx) = env::var(ENV_AFL_CXX) {
            new_args.push(afl_cxx);
        } else {
            new_args.push(FILE_BIN_CXX.to_string());
        }
    } else {
        if let Ok(afl_cc) = env::var(ENV_AFL_CC) {
            new_args.push(afl_cc);
        } else {
            new_args.push(FILE_BIN_CC.to_string());
        }
    }

    // Two ways to compile afl-clang-fast
    #[cfg(feature = "USE_TRACE_PC")]
    {
        new_args.push("-fsanitize-coverage=trace-pc-guard".to_string());
        new_args.push("-mllvm".to_string());
        new_args.push("-sanitizer-coverage-block-threshold=0".to_string());
        warnf("Disabling AFLGO features..\n");
    }
    #[cfg(not(feature = "USE_TRACE_PC"))]
    {
        new_args.push("-Xclang".to_string());
        new_args.push("-load".to_string());
        new_args.push("-Xclang".to_string());
        new_args.push(afl_config_files[FILE_LLVM_PASS_KEY].clone());
    }

    new_args.push("-Qunused-arguments".to_string());

    // flags
    let (mut dis_set, mut tar_set, mut out_set, mut add_set) = (false, false, false, false);

    let mut add_args_copy: Vec<String> = Vec::new();
    if check_flag {
        // AFLGo arguments
        if let Ok(dis_env_val) = env::var(ENV_AFLGO_DISTANCE_FILE) {
            new_args.push("-mllvm".to_string());
            new_args.push(format!("{}={}", ARG_DISTANCE_PROMPT, dis_env_val));
            dis_set = true;
        }
        if let Ok(tar_env_val) = env::var(ENV_AFLGO_TARGETS_FILE) {
            new_args.push("-mllvm".to_string());
            new_args.push(format!("{}={}", ARG_TARGETS_PROMPT, tar_env_val));
            tar_set = true;
        }
        if let Ok(out_env_val) = env::var(ENV_AFLGO_OUTDIR) {
            new_args.push("-mllvm".to_string());
            new_args.push(format!("{}={}", ARG_OUTDIR_PROMPT, out_env_val));
            out_set = true;
        }

        // AFLGo additional arguments
        if let Ok(add_env_val) = env::var(ENV_AFLGO_ADDITIONAL_PARAMS) {
            let add_args: Vec<String> = add_env_val
                .split_whitespace()
                .map(|s| s.to_string())
                .collect();

            for add_arg in add_args {
                if cmp_first_chars(&add_arg, ARG_DISTANCE_PROMPT, ARG_DISTANCE_PROMPT_LEN) {
                    if dis_set {
                        continue;
                    } else {
                        new_args.push("-mllvm".to_string());
                        dis_set = true;
                    }
                }
                if cmp_first_chars(&add_arg, ARG_TARGETS_PROMPT, ARG_TARGETS_PROMPT_LEN) {
                    if tar_set {
                        continue;
                    } else {
                        new_args.push("-mllvm".to_string());
                        tar_set = true;
                    }
                }
                if cmp_first_chars(&add_arg, ARG_OUTDIR_PROMPT, ARG_OUTDIR_PROMPT_LEN) {
                    if out_set {
                        continue;
                    } else {
                        new_args.push("-mllvm".to_string());
                        out_set = true;
                    }
                }

                new_args.push(add_arg.clone());
                add_args_copy.push(add_arg.clone());
            }

            add_set = true;
        }
    }

    let (
        mut maybe_linking,
        mut bit_mode,
        mut x_set,
        mut asan_set,
        mut msan_set,
        mut ubsan_set,
        mut fortify_set,
    ) = (true, 0, false, false, false, false, false);

    // Detect stray -v calls from ./configure scripts.
    if args.len() == 2 && args[1].eq("-v") {
        maybe_linking = false;
    }

    // traverse
    for i in 1..args.len() {
        // AFLGo options
        if cmp_first_chars(&args[i], ARG_DISTANCE_PROMPT, ARG_DISTANCE_PROMPT_LEN) {
            if check_flag && dis_set {
                continue;
            } else {
                new_args.push("-mllvm".to_string());
                dis_set = true;
            }
        }
        if cmp_first_chars(&args[i], ARG_TARGETS_PROMPT, ARG_TARGETS_PROMPT_LEN) {
            if check_flag && tar_set {
                continue;
            } else {
                new_args.push("-mllvm".to_string());
                tar_set = true;
            }
        }
        if cmp_first_chars(&args[i], ARG_OUTDIR_PROMPT, ARG_OUTDIR_PROMPT_LEN) {
            if check_flag && out_set {
                continue;
            } else {
                new_args.push("-mllvm".to_string());
                out_set = true;
            }
        }

        // Additional options
        if check_flag && add_set && cmp_with_group_strings(&args[i], &add_args_copy) {
            continue;
        }

        // 32-bit or 64-bit
        if args[i].eq("-m32") {
            bit_mode = 32;
        }
        if args[i].eq("-m64") {
            bit_mode = 64;
        }

        // x
        if args[i].eq("-x") {
            x_set = true;
        }

        // detect compiling rather than linking
        if args[i].eq("-c") || args[i].eq("-S") || args[i].eq("-E") {
            maybe_linking = false;
        }

        // Sanitizer
        if let Some(san_prefix) = args[i].get(0..ARG_SANITIZER_PROMPT_LEN) {
            if san_prefix.eq(ARG_SANITIZER_PROMPT) {
                if let Some(san_postfix) = args[i].get(ARG_SANITIZER_PROMPT_LEN..) {
                    let san_items: Vec<&str> = san_postfix.split(",").collect();
                    for san_item in san_items {
                        if san_item.eq("address") {
                            asan_set = true;
                        } else if san_item.eq("memory") {
                            msan_set = true;
                        } else if san_item.eq("undefined") {
                            ubsan_set = true;
                        }
                    }
                }
            }
        }

        // detect FORTIFY_SOURCE
        if args[i].contains("FORTIFY_SOURCE") {
            fortify_set = true;
        }

        // shared
        if args[i].eq("-shared") {
            maybe_linking = false;
        }

        // remove some options
        if args[i].eq("-Wl,-z,defs") || args[i].eq("-Wl,--no-undefined") {
            continue;
        }

        new_args.push(args[i].clone());
    }

    // AFL harden
    if let Ok(_) = env::var(ENV_AFL_HARDEN) {
        new_args.push("-fstack-protector-all".to_string());
        if !fortify_set {
            new_args.push("-D_FORTIFY_SOURCE=2".to_string());
        }
    }

    // ASan, MSan
    if !asan_set && !msan_set {
        if let Ok(_) = env::var(ENV_AFL_USE_ASAN) {
            if let Ok(_) = env::var(ENV_AFL_USE_MSAN) {
                fatal(
                    "ASan and MSan are mutually exclusive.",
                    std::panic::Location::caller(),
                );
            }
            if let Ok(_) = env::var(ENV_AFL_HARDEN) {
                fatal(
                    format!("ASan and {} are mutually exclusive.", ENV_AFL_HARDEN),
                    std::panic::Location::caller(),
                );
            }
            new_args.push("-U_FORTIFY_SOURCE".to_string());
            new_args.push("-fsanitize=address".to_string());
            asan_set = true;
        } else if let Ok(_) = env::var(ENV_AFL_USE_MSAN) {
            if let Ok(_) = env::var(ENV_AFL_USE_ASAN) {
                fatal(
                    "MSan and ASan are mutually exclusive.",
                    std::panic::Location::caller(),
                );
            }
            if let Ok(_) = env::var(ENV_AFL_HARDEN) {
                fatal(
                    format!("MSan and {} are mutually exclusive.", ENV_AFL_HARDEN),
                    std::panic::Location::caller(),
                );
            }
            new_args.push("-U_FORTIFY_SOURCE".to_string());
            new_args.push("-fsanitize=memory".to_string());
            msan_set = true;
        }
    }
    // UBSan
    if !ubsan_set {
        if let Ok(_) = env::var(ENV_AFL_USE_UBSAN) {
            if !asan_set && !msan_set {
                new_args.push("-U_FORTIFY_SOURCE".to_string());
            }
            new_args.push("-fsanitize=undefined".to_string());
            ubsan_set = true;
        }
    }

    #[cfg(feature = "USE_TRACE_PC")]
    {
        if let Ok(_) = env::var(ENV_AFL_INST_RATIO) {
            fatal(
                format!(
                    "'{}' is not available at compile time with 'trace-pc'.",
                    ENV_AFL_INST_RATIO
                ),
                std::panic::Location::caller(),
            )
        }
    }

    // AFL optimization
    if let Ok(_) = env::var(ENV_AFL_NOT_OPTIMIZE) {
        // do nothing
    } else {
        new_args.push("-g".to_string());
        new_args.push("-funroll-loops".to_string());
    }

    // AFL no built-in
    if let Ok(_) = env::var(ENV_AFL_NO_BUILTIN) {
        new_args.push("-fno-builtin-strcmp".to_string());
        new_args.push("-fno-builtin-strncmp".to_string());
        new_args.push("-fno-builtin-strcasecmp".to_string());
        new_args.push("-fno-builtin-strncasecmp".to_string());
        new_args.push("-fno-builtin-memcmp".to_string());
    }

    new_args.push("-D__AFL_HAVE_MANUAL_CONTROL=1".to_string());
    new_args.push("-D__AFL_COMPILER=1".to_string());
    new_args.push("-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION=1".to_string());

    // attribute
    // refer to https://github.com/google/AFL/blob/master/llvm_mode/afl-clang-fast.c#L231
    let mut afl_loop_attr = String::new();
    afl_loop_attr += "-D__AFL_LOOP(_A)=";
    afl_loop_attr += "({ static volatile char *_B __attribute__((used)); ";
    afl_loop_attr += &format!(" _B = (char*)\"{}\"; ", MACRO_PERSIST_SIG);
    #[cfg(target_vendor = "apple")]
    {
        afl_loop_attr += "__attribute__((visibility(\"default\"))) ";
        afl_loop_attr += "int _L(unsigned int) __asm__(\"___afl_persistent_loop\"); ";
    }
    #[cfg(not(target_vendor = "apple"))]
    {
        afl_loop_attr += "__attribute__((visibility(\"default\"))) ";
        afl_loop_attr += "int _L(unsigned int) __asm__(\"__afl_persistent_loop\"); ";
    }
    afl_loop_attr += "_L(_A); })";
    new_args.push(afl_loop_attr.clone());

    let mut afl_init_attr: String = String::new();
    afl_init_attr += "-D__AFL_INIT()=";
    afl_init_attr += "do { static volatile char *_A __attribute__((used)); ";
    afl_init_attr += &format!(" _A = (char*)\"{}\"; ", MACRO_DEFER_SIG);
    #[cfg(target_vendor = "apple")]
    {
        afl_init_attr += "__attribute__((visibility(\"default\"))) ";
        afl_init_attr += "void _I(void) __asm__(\"___afl_manual_init\"); ";
    }
    #[cfg(not(target_vendor = "apple"))]
    {
        afl_init_attr += "__attribute__((visibility(\"default\"))) ";
        afl_init_attr += "void _I(void) __asm__(\"__afl_manual_init\"); ";
    }
    afl_init_attr += "_I(); } while (0)";
    new_args.push(afl_init_attr.clone());

    // linking options
    if maybe_linking {
        if x_set {
            new_args.push("-x".to_string());
            new_args.push("none".to_string());
        }

        match bit_mode {
            32 => {
                if !Path::new(&afl_config_files[FILE_LLVM_RT32_KEY]).exists() {
                    fatal(
                        "-m32 is not supported by your compiler",
                        std::panic::Location::caller(),
                    );
                }
                new_args.push(afl_config_files[FILE_LLVM_RT32_KEY].clone());
            }
            64 => {
                if !Path::new(&afl_config_files[FILE_LLVM_RT64_KEY]).exists() {
                    fatal(
                        "-m64 is not supported by your compiler",
                        std::panic::Location::caller(),
                    );
                }
                new_args.push(afl_config_files[FILE_LLVM_RT64_KEY].clone());
            }
            _ => {
                new_args.push(afl_config_files[FILE_LLVM_RT_KEY].clone());
            }
        }
    }
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut cur_exe_path: PathBuf = PathBuf::new();
    let mut cur_exe_dir: &Path = Path::new("");
    let mut afl_config_files: HashMap<&str, String> = HashMap::new();
    let mut checked_args: Vec<String> = Vec::new();

    // current executable
    if let Ok(exe_path) = env::current_exe() {
        cur_exe_path.clone_from(&exe_path);
    } else {
        fatal(
            "Unable to find the current executable",
            std::panic::Location::caller(),
        );
    }

    // directory of current executable
    if let Some(exe_dir) = cur_exe_path.parent() {
        cur_exe_dir.clone_from(&exe_dir);
    } else {
        fatal(
            "Unable to find the directory where the current executable exists",
            std::panic::Location::caller(),
        );
    }

    // print title
    if let Ok(_) = env::var(ENV_AFL_QUIET) {
        // do nothing
    } else {
        title();
    }

    // print helper information
    if args.len() < 3 {
        let mut tmp_afl_cc: String = FILE_AFL_CC.to_string();
        let mut tmp_afl_cxx: String = FILE_AFL_CXX.to_string();
        if let Some(cur_afl_cc) = cur_exe_dir.join(FILE_AFL_CC).to_str() {
            tmp_afl_cc = cur_afl_cc.to_string();
        }
        if let Some(cur_afl_cxx) = cur_exe_dir.join(FILE_AFL_CXX).to_str() {
            tmp_afl_cxx = cur_afl_cxx.to_string();
        }
        outln("");
        outln("This is a helper application for afl-fuzz. It serves as a drop-in replacement");
        outln("for clang, letting you recompile third-party code with the required runtime");
        outln("instrumentation. A common use pattern would be one of the following:");
        outln("");
        outln(format!("  CC={} ./configure", tmp_afl_cc));
        outln(format!("  CXX={} ./configure", tmp_afl_cxx));
        outln("");
        outln("In contrast to the traditional afl-clang tool, this version is implemented as");
        outln("an LLVM pass and tends to offer improved performance with slow programs.");
        outln("");
        outln("You can specify custom next-stage toolchain via AFL_CC and AFL_CXX. Setting");
        outln("AFL_HARDEN enables hardening optimizations in the compiled code.");
        outln("");
        outln(format!(
            "FGo adds options '{}', '{} and '{}'.",
            ARG_DISTANCE_PROMPT, ARG_TARGETS_PROMPT, ARG_OUTDIR_PROMPT
        ));
        outln(format!(
            "Options '{}' and '{}' is used for preprocessing mode",
            ARG_TARGETS_PROMPT, ARG_OUTDIR_PROMPT
        ));
        outln(format!(
            " and option '{}' is used for instrumentation mode.",
            ARG_DISTANCE_PROMPT
        ));
        outln("");
        outln("The Rust edition adds some environment variables to check options");
        outln(" and be able to get AFLGo options from environment variables.");
        outln("You can find more information at https://github.com/harvey-lau/fgo#addition");
        outln("");
        process::exit(1);
    }

    // find objects
    find_objs(cur_exe_dir, &mut afl_config_files);

    // check parameter arguments
    check_edit_params(&args, &mut checked_args, &afl_config_files);

    // execute compiler
    let mut command = Command::new(checked_args[0].clone());
    for i in 1..checked_args.len() {
        command.arg(checked_args[i].clone());
    }
    let output = command
        .stdout(Stdio::inherit())
        .stderr(Stdio::inherit())
        .output()
        .expect("failed to execute process");
    if let Some(exit_code) = output.status.code() {
        if exit_code == 0 {
            assert!(output.status.success());
        }
        process::exit(exit_code);
    }

    /*
    println!("Program: {}", args[0]);
    for i in 1..args.len() {
        println!("Arg {}: {}", i, args[i]);
    }
    */
}
