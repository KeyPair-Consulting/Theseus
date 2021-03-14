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

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-counter-bitwidth <filename>\n");
  fprintf(stderr, "Extract deltas under the assumption that the data is an increasing counter of some inferred bitwidth.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t *delta = NULL;
  uint32_t rawmax;
  uint64_t nextpower;
  uint32_t min;
  uint32_t max;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if ((datalen = readuint32file(infp, &data)) < 1) {
    useageExit();
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %" PRId64 " uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
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

  fprintf(stderr, "Next binary power: %" PRId64 " (assuming a %u bit counter)\n", nextpower, __builtin_popcount(rawmax));

  if ((delta = malloc(datalen * sizeof(uint32_t))) == NULL) {
    perror("Can't allocate extra memory");
    exit(EX_OSERR);
  }

  max = 0;
  min = UINT32_MAX;

  if (datalen < 2) {
    fprintf(stderr, "Too little data\n");
    exit(EX_DATAERR);
  }

  for (size_t i = 1; i < datalen; i++) {
    uint64_t deltamin;

    if (data[i] >= data[i - 1]) {
      deltamin = data[i] - data[i - 1];
    } else {
      deltamin = ((uint64_t)data[i] + nextpower) - (uint64_t)data[i - 1];
    }

    assert(deltamin <= UINT32_MAX);

    delta[i - 1] = (uint32_t)deltamin;
    if (deltamin < min) min = (uint32_t)deltamin;
    if (deltamin > max) max = (uint32_t)deltamin;
  }

  assert(max >= min);
  fprintf(stderr, "min diff: %u, max diff: %u\n", min, max);
  fprintf(stderr, "Translating min diff to 0 (new max diff: %u)\n", max - min);

  for (size_t i = 0; i < datalen - 1; i++) {
    assert(delta[i] >= min);
    data[i] = delta[i] - min;
  }

  if (fwrite(data, sizeof(uint32_t), datalen - 1, stdout) != datalen - 1) {
    if (delta != NULL) free(delta);
    delta = NULL;
    if (data != NULL) free(data);
    data = NULL;
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  if (delta != NULL) free(delta);
  delta = NULL;
  if (data != NULL) free(data);
  data = NULL;
  return (0);
}
