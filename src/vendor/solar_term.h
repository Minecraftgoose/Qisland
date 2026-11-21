/* Qisland vendored copy — MODIFIED: #include "date.h" -> "lunar_date.h".
 * Upstream: https://github.com/iseemoon/lunar-calendar  (Apache-2.0)
 */
#pragma once

#include "lunar_date.h"

#ifdef __cplusplus
extern "C" {
#endif

// 计算某年的第n个节气为几日(从小寒开始，n取值范围：0～23)
// y >= 1900
const char * get_day_of_solar_term(uint32_t y, uint8_t n, LUNAR_DATE * solar_date);

#ifdef __cplusplus
}
#endif