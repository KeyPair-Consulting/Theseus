/* This file is part of the Theseus distribution.
 * Copyright 2020-2024 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef ENTTYPES_H
#define ENTTYPES_H

#include <stdint.h>

#ifndef DBL_INFINITY
#define DBL_INFINITY __builtin_inf()
#endif

#ifdef U32STATDATA
#define statData_t uint32_t
#define signedStatData_t int64_t
#define STATDATA_MAX UINT32_MAX
#define STATDATA_BITS 32U
#define STATDATA_STRING "uint32_t"
#else
#define statData_t uint8_t
#define signedStatData_t int16_t
#define STATDATA_MAX UINT8_MAX
#define STATDATA_BITS 8U
#define STATDATA_STRING "uint8_t"
#endif

#endif
