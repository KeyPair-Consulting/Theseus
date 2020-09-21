/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef SA_H
#define SA_H

#include <stdint.h>  // for INT32_MAX, uint32_t
#include <sys/types.h>  // for int32_t

#include "entlib.h"

#define SAINDEX int32_t
#define SAINDEX_MAX INT32_MAX

void calcSALCP(const statData_t *inData, size_t n, size_t k, SAINDEX *SA, SAINDEX *LCP);
#endif
