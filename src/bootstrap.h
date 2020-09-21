/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "fancymath.h"
#include "randlib.h"

double calculatePercentile(double p, double *data, size_t datalen, double validMin, double validMax);
double BCaBootstrapPercentile(double p, double *data, size_t datalen, double validMin, double validMax, double *confidenceInterval, size_t rounds, double alpha, struct randstate *rstate);
double calculateMean(double *data, size_t datalen);
double BCaBootstrapMean(double *data, size_t datalen, double validMin, double validMax, double *confidenceInterval, size_t rounds, double alpha, struct randstate *rstate);
size_t aboveValue(double value, const double *data, size_t datalen);
size_t belowValue(double value, const double *data, size_t datalen);
size_t trimDoubleRange(double **dataptr, size_t datalen, double validMin, double validMax);
double processedCalculatePercentile(double p, double *data, size_t datalen, bool sorted, int extraVerbose);

#endif
