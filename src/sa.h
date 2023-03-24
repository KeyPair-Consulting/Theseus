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

#include <divsufsort.h>
#include <divsufsort64.h>

#include "entlib.h"

#define SAIDX_MAX INT32_MAX
#define SAIDX64_MAX INT64_MAX

void calcSALCP(const statData_t *inData, size_t n, size_t k, saidx_t *SA, saidx_t *LCP);
void calcSALCP64(const statData_t *inData, size_t n, size_t k, saidx64_t *SA, saidx64_t *LCP);
#endif
