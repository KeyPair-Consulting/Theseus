/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef HASHMODULUS_H
#define HASHMODULUS_H

#include "enttypes.h"

#define MODULUSCOUNT 8
// These are prime values, roughly doubling in size for each step
static const signedStatData_t hashModulus[MODULUSCOUNT] = {1, 2, 5, 11, 31, 67, 127, 0};

#endif
