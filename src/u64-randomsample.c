/* This file is part of the Theseus distribution.
 * Copyright 2024 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
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
#include <string.h>
#include <sysexits.h>
#include <errno.h>

#include "binio.h"
#include "randlib.h"
#include "globals-inst.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u64-randomsample inputfile outputSamples\n");
  fprintf(stderr, "Produce a random sample of the input file of size outputSamples.\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint64_ts\n");
  fprintf(stderr, "output is to stdout, and is u64 ints\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint64_t *data = NULL;
  uint64_t *outputData = NULL;
  size_t datalen;
  size_t outputDataLen;
  struct randstate rstate;
  unsigned long long inint;

  initGenerator(&rstate);
  seedGenerator(&rstate);

  if (argc != 3) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  inint = strtoull(argv[2], NULL, 0);
  if (((inint == ULLONG_MAX) && errno == ERANGE) || (errno == EINVAL) || (inint > SIZE_MAX) || (inint == 0)) {
    useageExit();
  }
  outputDataLen = (size_t)inint;

  datalen = readuint64file(infp, &data);
  if (datalen < 1) {
    useageExit();
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu samples\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  if ((outputData = malloc(outputDataLen * sizeof(uint64_t))) == NULL) {
      perror("Can't allocate array for output data");
      exit(EX_OSERR);
  }

  fprintf(stderr, "Randomly selecting data\n");
  for(size_t i = 0; i < outputDataLen; i++) {
    outputData[i] = data[randomRange64(datalen-1, &rstate)];
  }
  free(data);


  fprintf(stderr, "Outputting the data...\n");
  if (fwrite(outputData, sizeof(uint64_t), outputDataLen, stdout) != outputDataLen) {
    perror("Can't write output to stdout");
    exit(EX_OSERR);
  }

  free(outputData);
  return EX_OK;
}
