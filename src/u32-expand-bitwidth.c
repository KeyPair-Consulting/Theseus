/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

#define ABSDIFF(x, y) ((x) >= (y)) ? ((x) - (y)) : ((y) - (x))

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-expand-bitwidth <filename>\n");
  fprintf(stderr, "Extract inferred values under the assumption that the data is a truncation of some sampled value, whose bitwidth is inferred.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint64_t *transdata = NULL;
  uint32_t rawmax;
  uint64_t nextpower;
  uint64_t maxdiff;
  uint64_t delta, delta0, delta1, max, min;
  size_t maxindex;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Read in %" PRId64 " uint32_ts\n", datalen);

  if (datalen < 2) {
    fprintf(stderr, "Too little data\n");
    exit(EX_DATAERR);
  }

  rawmax = 0;
  for (size_t i = 0; i < datalen; i++)
    if (data[i] > rawmax) rawmax = data[i];

  rawmax |= rawmax >> 1;
  rawmax |= rawmax >> 2;
  rawmax |= rawmax >> 4;
  rawmax |= rawmax >> 8;
  rawmax |= rawmax >> 16;
  nextpower = rawmax + 1;

  fprintf(stderr, "Next binary power: %" PRId64 " (assuming a %u bit value)\n", nextpower, __builtin_popcount(rawmax));

  if ((transdata = malloc(datalen * sizeof(uint64_t))) == NULL) {
    perror("Can't allocate extra memory");
    exit(EX_OSERR);
  }

  for (size_t i = 0; i < datalen; i++) transdata[i] = UINT64_MAX;

  maxdiff = 0;
  maxindex = 0;

  // Find the biggest difference; this is likely to be a transition point.
  for (size_t i = 1; i < datalen; i++) {
    uint32_t curdiff = ABSDIFF(data[i], data[i - 1]);
    if (curdiff > maxdiff) {
      maxindex = i;
      maxdiff = curdiff;
    }
  }

  assert(maxindex > 0);
  fprintf(stderr, "Found most obvious transition at index: %zu (0x%08X -> 0x%08X)\n", maxindex, data[maxindex - 1], data[maxindex]);

  // Make the inference for the most obvious transition
  delta = ABSDIFF(((uint64_t)data[maxindex]), ((uint64_t)data[maxindex - 1]));
  delta0 = ABSDIFF(((uint64_t)data[maxindex]) + nextpower, ((uint64_t)data[maxindex - 1]));
  delta1 = ABSDIFF(((uint64_t)data[maxindex]), ((uint64_t)data[maxindex - 1]) + nextpower);
  if ((delta <= delta0) && (delta <= delta1)) {
    transdata[maxindex] = data[maxindex];
    transdata[maxindex - 1] = data[maxindex - 1];
  } else if ((delta0 <= delta) && (delta0 <= delta1)) {
    transdata[maxindex] = data[maxindex] + nextpower;
    transdata[maxindex - 1] = data[maxindex - 1];
  } else {
    transdata[maxindex] = data[maxindex];
    transdata[maxindex - 1] = data[maxindex - 1] + nextpower;
  }

  fprintf(stderr, "Now %010" PRIx64 " -> 0x%010" PRIx64 "\n", transdata[maxindex - 1], transdata[maxindex]);

  // Make the inference for earlier data
  for (size_t i = maxindex - 1; i > 0; i--) {
    // transdata[i] is populated. Make an inference about data[i-1]
    delta = ABSDIFF(transdata[i], ((uint64_t)data[i - 1]));
    delta1 = ABSDIFF(transdata[i], ((uint64_t)data[i - 1]) + nextpower);
    if ((delta <= delta1)) {
      transdata[i - 1] = data[i - 1];
    } else {
      transdata[i - 1] = data[i - 1] + nextpower;
    }
  }

  // Make the inference for later data
  for (size_t i = maxindex; i < datalen - 1; i++) {
    // transdata[i] is populated. Make an inference about data[i+1]
    delta = ABSDIFF(transdata[i], ((uint64_t)data[i + 1]));
    delta1 = ABSDIFF(transdata[i], ((uint64_t)data[i + 1]) + nextpower);
    if ((delta <= delta1)) {
      transdata[i + 1] = data[i + 1];
    } else {
      transdata[i + 1] = data[i + 1] + nextpower;
    }
  }

  max = 0;
  min = UINT64_MAX;
  maxindex = 0;
  for (size_t i = 0; i < datalen; i++) {
    if (transdata[i] > max) {
      max = transdata[i];
      maxindex = i;
    }
    if (transdata[i] < min) min = transdata[i];
  }

  fprintf(stderr, "New max %zu (index %zu)\n", max, maxindex);
  fprintf(stderr, "New min %zu\n", min);

  if (fwrite(transdata, sizeof(uint64_t), datalen, stdout) != datalen) {
    if (transdata != NULL) free(transdata);
    transdata = NULL;
    if (data != NULL) free(data);
    data = NULL;
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  if (transdata != NULL) free(transdata);
  transdata = NULL;
  if (data != NULL) free(data);
  data = NULL;
  return (0);
}
