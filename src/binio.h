/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef BINIO_H
#define BINIO_H

#include <stdint.h>
#include "entlib.h"

size_t readuint32file(FILE *input, uint32_t **buffer);
size_t readuint64file(FILE *input, uint64_t **buffer);
size_t readuintfile(FILE *input, statData_t **buffer);
size_t readuintfileloc(FILE *input, statData_t **buffer, size_t subsetIndex, size_t subsetSize);
size_t readdoublefile(FILE *input, double **buffer);
size_t readasciidoubles(FILE *input, double **buffer);
void mergeSortedLists(const double *in1, size_t len1, const double *in2, size_t len2, double *out);
size_t readasciidoublepoints(FILE *input, double **buffer);
size_t readasciiuint64s(FILE *input, uint64_t **buffer);
#endif
