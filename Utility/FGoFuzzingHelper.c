/**
 *
 *
 *
 */

#include "FGoFuzzingHelper.h"

#include "../AFL-Fuzz/debug.h"
#include "FGoDefs.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef PARSER_LIBRARY_PATH
    #define PARSER_LIBRARY_PATH "fgo-parser.so"
#endif

#ifndef PARSER_FUNCTION_NAME
    #define PARSER_FUNCTION_NAME "parse"
#endif

#ifndef PARSER_ERROR_FUNCTION_NAME
    #define PARSER_ERROR_FUNCTION_NAME "parse_error"
#endif

#ifndef SAFE_FREE
    #define SAFE_FREE(ptr)                                                                     \
        do {                                                                                   \
            if ((ptr) != NULL) {                                                               \
                free(ptr);                                                                     \
                (ptr) = NULL;                                                                  \
            }                                                                                  \
        } while (0)
#endif

void helper_load_target_info(const char *info_dir, target_info_t *target_info)
{
    if (!info_dir) {
        FATAL("Null pointer of parameter 'info_dir'");
    }
    if (!target_info) {
        FATAL("Null pointer of parameter 'target_info'");
    }

    void *handle = NULL;
    int (*parse)(const char *, target_info_t *);
    const char *(*parse_error)(void);
    char *error;

    handle = dlopen(PARSER_LIBRARY_PATH, RTLD_LAZY);
    if (!handle) FATAL("Failed to open the library '%s'", PARSER_LIBRARY_PATH);
    parse = dlsym(handle, PARSER_FUNCTION_NAME);
    if ((error = dlerror()) != NULL)
        FATAL(
            "Failed to get the function named '%s' from '%s'. Error: %s", PARSER_FUNCTION_NAME,
            PARSER_LIBRARY_PATH, error
        );
    parse_error = dlsym(handle, PARSER_ERROR_FUNCTION_NAME);
    if ((error = dlerror()) != NULL)
        FATAL(
            "Failed to get the function named '%s' from '%s'. Error: %s",
            PARSER_ERROR_FUNCTION_NAME, PARSER_LIBRARY_PATH, error
        );

    target_info->target_count = 0;
    target_info->target_start = NULL;
    target_info->quantile_size = NULL;
    target_info->target_quantile = NULL;
    if (0 != (*parse)(info_dir, target_info))
        FATAL("Failed to parse target information. Error: %s", parse_error());

    if (0 != dlclose(handle))
        FATAL("Failed to open the handler of the library '%s'", PARSER_LIBRARY_PATH);
}

double helper_get_quantile(
    const target_info_t *target_info, uint32_t target_id, uint32_t distance
)
{
    uint32_t quantile_index = distance - target_info->target_start[target_id];
    if (quantile_index < 0) return 0.0;
    else if (quantile_index >= target_info->quantile_size[target_id]) return 1.0;
    else return target_info->target_quantile[target_id][quantile_index];
}

void helper_free_target_info(target_info_t *target_info)
{
    if (target_info->target_quantile) {
        for (uint32_t i = 0; i < target_info->target_count; ++i)
            SAFE_FREE(target_info->target_quantile[i]);
    }
    SAFE_FREE(target_info->target_quantile);
    SAFE_FREE(target_info->quantile_size);
    SAFE_FREE(target_info->target_start);
}