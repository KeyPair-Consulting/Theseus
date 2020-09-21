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
  fprintf(stderr, "u32-discard-fixed-bits <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output sent to stdout are uint32_tegers, with all fixed bits removed and the non-fixed bits moved to the LSB of the output.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  size_t i;
  uint32_t bitmask;
  uint32_t maxval, minval;
  uint32_t bits;
  double doubleBits;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);
  assert(datalen > 0);

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  maxval = 0;
  minval = UINT_MAX;

  for (i = 0; i < datalen; i++) {
    if (data[i] > maxval) {
      maxval = data[i];
    }
    if (data[i] < minval) {
      minval = data[i];
    }
  }

  doubleBits = ceil(log2((double)maxval + 1.0));
  assert(doubleBits >= 0.0);
  bits = (uint32_t)doubleBits;

  bitmask = getActiveBits(data, datalen);

  fprintf(stderr, "Symbols in the range [%u, %u], %u bit, bitmask: 0x%08X\n", minval, maxval, bits, bitmask);

  fprintf(stderr, "Outputting data\n");
  for (i = 0; i < datalen; i++) {
    data[i] = extractbits(data[i], bitmask);
  }

  if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
    perror("Can't write output to stdout");
    exit(EX_OSERR);
  }

  free(data);

  return EX_OK;
}
