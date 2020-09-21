/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef TRANSLATE_H
#define TRANSLATE_H
#include <stddef.h>
#include <stdint.h>
#include "entlib.h"
bool translate(statData_t *S, size_t L, size_t *k, double *translatedMedian);
#endif
