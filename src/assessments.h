/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef ASSESSMENTS_h
#define ASSESSMENTS_h

#include <stdbool.h>
#include <stddef.h>

#include "entlib.h"
#include "randlib.h"

double bootstrapAssessments(struct entropyTestingResult *result, size_t count, size_t bitWidth, struct randstate *rstate);
double bootstrapParameters(struct entropyTestingResult *result, size_t count, size_t bitWidth, struct randstate *rstate);

#endif
