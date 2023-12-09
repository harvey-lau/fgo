/**
 *
 *
 */

#include "FGoFuzzingHelper.h"

#ifdef __cplusplus
extern "C" {
#endif

int parse(const char *info_dir, target_info_t *target_info);

const char *parse_error(void);

#ifdef __cplusplus
}
#endif

// g++ -std=c++17 -I/root/indicators/include -I/root/jsoncpp/include
// -L/root/jsoncpp/Debug-build/lib -fpic -shared FGoFuzzingUtils.cpp -o fgo-parser.so
// -l:libjsoncpp.a