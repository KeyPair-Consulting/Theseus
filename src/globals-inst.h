/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef GLOBALSINST_H
#define GLOBALSINST_H
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include "entlib.h"
#include "globals.h"

int configVerbose = 0;
bool configBootstrapParams = false;
size_t configThreadCount = 0;
double globalErrors[ERRORSLOTS] = {-1.0};
char errorLabels[ERRORSLOTS][LABELLEN] = {0};

double configBootstrapConfidence = 0.99;
size_t configBootstrapRounds = 15000;
#endif
