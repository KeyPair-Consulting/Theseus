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
#include <unistd.h>

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "bits-in-use <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to consist of uint32_ts\n");
  fprintf(stderr, "outputs the number of bits required to represent data after removing stuck and superflous bits.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t bitmask;
  uint32_t outputBits;

  configVerbose = 2;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if((datalen = readuint32file(infp, &data)) < 1) {
    perror("File contains no data");
    exit(EX_DATAERR);
  }
  assert(data != NULL);

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  bitmask = getActiveBits(data, datalen);

  fprintf(stderr, "Found bitmask 0x%08x\n", bitmask);

  outputBits = (uint32_t)(__builtin_popcountl(bitmask));

  printf("%u\n", outputBits);

  free(data);

  return EX_OK;
}
