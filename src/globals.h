/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef GLOBALS_H
#define GLOBALS_H

#define ERRORSLOTS 16
#define LABELLEN 64

#include <stdbool.h>
#include <stddef.h>

extern int configVerbose;
extern bool configBootstrapParams;
extern size_t configThreadCount;
extern double globalErrors[ERRORSLOTS];
extern char errorLabels[ERRORSLOTS][LABELLEN];

extern double configBootstrapConfidence;
extern size_t configBootstrapRounds;

#endif
