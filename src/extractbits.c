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
#include <unistd.h>

#include "binio.h"
#include "binutil.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "extractbits <inputfile> <bitmask>\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output is sent to stdout is in " STATDATA_STRING " format\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t outdata;
  uint32_t *data = NULL;
  size_t i;
  uint32_t outputBits;
  uint32_t bitmask;

  if (argc != 3) {
    fprintf(stderr, "Wrong number of arguments: argc=%d\n", argc);
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  bitmask = (uint32_t)strtoll(argv[2], NULL, 0);
  outputBits = (uint32_t)__builtin_popcount(bitmask);

  if ((outputBits == 0) || (outputBits > STATDATA_BITS)) {
    fprintf(stderr, "The number of outputBits is out of range: 0 > %u > %u\n", outputBits, STATDATA_BITS);
    useageExit();
  }

  fprintf(stderr, "Input bitmask 0x%X (Hamming weight: %u)\n", bitmask, outputBits);

  datalen = readuint32file(infp, &data);
  assert(data != NULL);
  assert(datalen > 0);
  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Outputting data\n");
  for (i = 0; i < datalen; i++) {
    outdata = (statData_t)extractbits(data[i], bitmask);
    if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
      perror("Can't write output to stdout");
      exit(EX_OSERR);
    }
  }

  free(data);

  return EX_OK;
}
