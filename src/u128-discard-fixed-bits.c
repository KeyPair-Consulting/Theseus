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
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "discard-fixed-bits-u128 inputfile outputgroup\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint128_t\n");
  fprintf(stderr, "outputgroup sent to stdout are uint32_tegers, with all fixed bits removed and the non-fixed bits moved to the LSB of the output.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t *dataPartition[4];
  size_t i, j;
  uint32_t bitmask[4];
  uint32_t maxval, minval;
  uint32_t bits;
  double doubleBits;
  int outputGroup;

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION(SIZE_MAX) > 32);

  if (argc != 3) {
    useageExit();
  }

  outputGroup = atoi(argv[2]);

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);
  assert(datalen > 0);
  assert((datalen % 4) == 0);

  dataPartition[0] = calloc(datalen / 4, sizeof(uint32_t));
  dataPartition[1] = calloc(datalen / 4, sizeof(uint32_t));
  dataPartition[2] = calloc(datalen / 4, sizeof(uint32_t));
  dataPartition[3] = calloc(datalen / 4, sizeof(uint32_t));

  if ((dataPartition[0] == NULL) || (dataPartition[1] == NULL) || (dataPartition[2] == NULL) || (dataPartition[3] == NULL)) {
    perror("Can't allocate partitions");
    exit(EX_OSERR);
  }

  for (j = 0; j < datalen; j++) {
    (dataPartition[j & 0x3])[j >> 2] = data[j];
  }

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  for (j = 0; j < 4; j++) {
    maxval = 0;
    minval = UINT_MAX;

    for (i = 0; i < datalen / 4; i++) {
      if ((dataPartition[j])[i] > maxval) {
        maxval = (dataPartition[j])[i];
      }
      if ((dataPartition[j])[i] < minval) {
        minval = (dataPartition[j])[i];
      }
    }
    doubleBits = ceil(log2((double)maxval + 1.0));
    assert(doubleBits >= 0.0);
    bits = (uint32_t)doubleBits;

    bitmask[j] = getActiveBits(dataPartition[j], datalen / 4);

    fprintf(stderr, "Symbols in the range [%u, %u] (%u bit: bitmask 0x%08X)\n", minval, maxval, bits, bitmask[j]);

    for (i = 0; i < datalen / 4; i++) {
      (dataPartition[j])[i] = extractbits((dataPartition[j])[i], bitmask[j]);
    }
  }

  fprintf(stderr, "%d bits total\n", __builtin_popcount(bitmask[0]) + __builtin_popcount(bitmask[1]) + __builtin_popcount(bitmask[2]) + __builtin_popcount(bitmask[3]));

  fprintf(stderr, "Outputting group %d\n", outputGroup);

  for (i = 0; i < datalen / 4; i++) {
    if (fwrite(&((dataPartition[outputGroup])[i]), sizeof(uint32_t), 1, stdout) != 1) {
      perror("Can't write output to stdout");
      exit(EX_OSERR);
    }
  }

  free(data);
  free(dataPartition[0]);
  free(dataPartition[1]);
  free(dataPartition[2]);
  free(dataPartition[3]);

  return EX_OK;
}
