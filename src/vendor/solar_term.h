/* Qisland vendored copy — MODIFIED: #include "date.h" -> "lunar_date.h". */
#pragma once

#include "lunar_date.h"

#ifdef __cplusplus
extern "C" {
#endif

const char * get_day_of_solar_term(uint32_t y, uint8_t n, LUNAR_DATE * solar_date);

#ifdef __cplusplus
}
#endif