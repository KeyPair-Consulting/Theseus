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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-selectrange inputfile low high\n");
  fprintf(stderr, "Extracts all values between low and high (inclusive).\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output is to stdout, and is u32 ints\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t *data = NULL;
  size_t i;
  size_t datalen;
  uint32_t lowValue, highValue;
  long long int inll;

  if (argc != 4) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  inll = strtoll(argv[2], NULL, 0);
  if ((inll < 0) || (inll > UINT_MAX)) {
    useageExit();
  } else {
    lowValue = (uint32_t)inll;
  }

  inll = strtoll(argv[3], NULL, 0);
  if ((inll < 0) || (inll > UINT_MAX) || (inll < lowValue)) {
    useageExit();
  } else {
    highValue = (uint32_t)inll;
  }

  datalen = readuint32file(infp, &data);
  if (datalen < 1) {
    useageExit();
  }
  assert(data != NULL);

  fprintf(stderr, "Read in %zu samples\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Outputting data in the interval [%u, %u]\n", lowValue, highValue);

  fprintf(stderr, "Outputting the data...\n");
  for (i = 0; i < datalen; i++) {
    if ((data[i] >= lowValue) && (data[i] <= highValue)) {
      if (fwrite(&(data[i]), sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write output to stdout");
        exit(EX_OSERR);
      }
    }
  }

  free(data);
  return EX_OK;
}
