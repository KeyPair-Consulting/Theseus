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
  fprintf(stderr, "discard-fixed-bits inputfile\n");
  fprintf(stderr, "inputfile is assumed to be a stream of " STATDATA_STRING " \n");
  fprintf(stderr, "output sent to stdout are " STATDATA_STRING ", with all fixed bits removed and the non-fixed bits moved to the lsb of the output.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data = NULL;
  uint32_t *u32data = NULL;
  uint32_t bitmask;
  statData_t maxval, minval;
  uint32_t bits;
  double doubleBits;

  configVerbose = 2;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuintfile(infp, &data);
  assert(data != NULL);
  assert(datalen > 0);

  if ((u32data = malloc(sizeof(uint32_t) * datalen)) == NULL) {
    perror("Can't allocate extra memory");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Read in %zu " STATDATA_STRING "s\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  maxval = 0;
  minval = STATDATA_MAX;

  for (size_t i = 0; i < datalen; i++) {
    if (data[i] > maxval) {
      maxval = data[i];
    }
    if (data[i] < minval) {
      minval = data[i];
    }
    u32data[i] = data[i];
  }

  doubleBits = ceil(log2((double)maxval + 1.0));
  assert(doubleBits >= 0.0);
  bits = (statData_t)doubleBits;

  bitmask = getActiveBits(u32data, datalen);

  fprintf(stderr, "Symbols in the range [%u, %u], %u bit, bitmask: 0x%08X\n", minval, maxval, bits, bitmask);

  fprintf(stderr, "Outputting data\n");
  for (size_t i = 0; i < datalen; i++) {
    uint32_t out;
    out = extractbits(u32data[i], bitmask);
    assert(out <= STATDATA_MAX);
    data[i] = (statData_t)out;
  }

  if (fwrite(data, sizeof(statData_t), datalen, stdout) != datalen) {
    perror("Can't write output to stdout");
    exit(EX_OSERR);
  }

  free(data);
  free(u32data);

  return EX_OK;
}
