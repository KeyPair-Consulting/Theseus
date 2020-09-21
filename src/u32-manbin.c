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
#include <errno.h>
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
#include "rbt_misc.h"
#include "red_black_tree.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-manbin inputfile cutoff_1 ... cutoff_{n-1}\n");
  fprintf(stderr, "convert data to one of the n bin numbers (0, ..., n-1).\n");
  fprintf(stderr, "The cutoffs specify the first value in the next bin (so the first bin is [0, cutoff_1), the second bin is [cutoff_1, cutoff_2), the last bin is [cutoff_{n-1}, UINT32_MAX ] etc.)\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output is to stdout, and is uint8_t values\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t *data = NULL;
  size_t i;
  int curarg;
  size_t bounds;
  size_t datalen;
  int64_t cutoffs[255];
  int64_t lowbound;

  for (i = 0; i < 255; i++) cutoffs[i] = -1;
  // The total number of bins must <= 256, so 
  // bounds + 1 <= 256
  // (argc-2) + 1 <= 257
  // argc <= 258
  if ((argc < 3) || (argc > 258)) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  for (curarg = 2; curarg < argc; curarg++) {
    unsigned long lout = strtoul(argv[curarg], NULL, 0);
    if ((lout > UINT32_MAX) || (errno != 0)) useageExit();
    cutoffs[curarg - 2] = (int64_t)lout;
  }

  bounds = (size_t)argc - 2;
  fprintf(stderr, "A total of %zu output bins.\n", bounds+1);
  assert(bounds <= UINT8_MAX);
  if (bounds < 1) useageExit();
  if(cutoffs[0] == 0) useageExit();
  if(cutoffs[bounds-1] > UINT32_MAX) useageExit();

  lowbound=0;

  for (i = 0; i < bounds; i++) {
    if (lowbound >= cutoffs[i]) {
      useageExit();
    } else {
      fprintf(stderr, "[ %ld, %ld ), ", lowbound, cutoffs[i]);
      lowbound = cutoffs[i];
    }
  }
  fprintf(stderr, "[ %ld, %ld ]\n", cutoffs[bounds-1], (int64_t)UINT32_MAX);

  datalen = readuint32file(infp, &data);
  if (datalen < 2) {
    useageExit();
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu samples\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Outputting the data...\n");
  for (i = 0; i < datalen; i++) {
    //set this to the last valid symbol
    uint8_t symbol = (uint8_t)bounds;
    for (uint8_t j = 0; j < bounds; j++) {
      if (cutoffs[j] > data[i]) {
        symbol = j;
        break;
      }
    }

    assert(symbol <= bounds);

    if (fwrite(&symbol, sizeof(uint8_t), 1, stdout) != 1) {
      perror("Can't write output to stdout");
      exit(EX_OSERR);
    }
  }

  free(data);
  return EX_OK;
}
