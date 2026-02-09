/* This file is part of the Theseus distribution.
 * Copyright 2025 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "globals-inst.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-mean <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint64_t sum = 0;
  double mean;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if ((datalen = readuint32file(infp, &data)) < 1) {
    perror("Data file is empty");
    exit(EX_DATAERR);
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  assert(datalen > 0);

  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  for(size_t i=0; i<datalen; i++) {
    uint64_t newsum;
    newsum = sum + data[i];
    assert(newsum >= sum);
    sum = newsum;
  }

  free(data);

  mean = ((double)sum) / ((double)datalen);
  printf("%.17g\n", mean);

  return EX_OK;
}
