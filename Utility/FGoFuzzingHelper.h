/**
 *
 *
 *
 */

#ifndef FGOFUZZINGHELPER_H_
#define FGOFUZZINGHELPER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __target_info_t
{
    uint32_t target_count;
    uint32_t *target_start;
    uint32_t *quantile_size;
    double **target_quantile;
} target_info_t;

/// @brief Load target information from a Json file under directory `info_dir`.
/// Exit via `PFATAL` when errors occur. It loads a dynamic link library when
/// calling a `parse` function to parse Json file.
/// @param info_dir
/// @param target_info
void helper_load_target_info(const char *info_dir, target_info_t *target_info);

/// @brief Get the quantile of a given distance of the NO.i target. This
/// function doesn't check the validity of the given pointers for fast invocation.
/// @param target_id
/// @param distance
/// @return
double helper_get_quantile(
    const target_info_t *target_info, uint32_t target_id, uint32_t distance
);

/// @brief Free the memory
/// @param target_info
void helper_free_target_info(target_info_t *target_info);

#ifdef __cplusplus
}
#endif

#endif