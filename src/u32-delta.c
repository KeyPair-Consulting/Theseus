/* This file is part of the Theseus distribution.
 * Copyright 2020-2024 Joshua E. Hill <josh@keypair.us>
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
  fprintf(stderr, "u32-delta <filename>\n");
  fprintf(stderr, "Extract deltas and then translate the result to a positive value.\n");
  fprintf(stderr, "The values are output via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  size_t i;
  int64_t *delta = NULL;
  int64_t min;
  int64_t max;
  int64_t newmax;
  int64_t curdelta;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  if ((delta = malloc(datalen * sizeof(int64_t))) == NULL) {
    perror("Can't allocate extra memory");
    exit(EX_OSERR);
  }

  max = INT64_MIN;
  min = INT64_MAX;

  if (datalen < 2) {
    fprintf(stderr, "Too little data\n");
    exit(EX_DATAERR);
  }

  for (i = 1; i < datalen; i++) {
    curdelta = (int64_t)data[i] - (int64_t)data[i - 1];

    delta[i - 1] = curdelta;
    if (curdelta < min) min = curdelta;
    if (curdelta > max) max = curdelta;
  }

  newmax = max - min;

  fprintf(stderr, "min diff: %" PRId64 ", max diff: %" PRId64 "\n", min, max);
  if (newmax > UINT32_MAX) {
    if (delta != NULL) free(delta);
    delta = NULL;
    if (data != NULL) free(data);
    data = NULL;
    fprintf(stderr, "Can't map this to the appropriate range\n");
    exit(EX_DATAERR);
  }

  for (i = 0; i < datalen - 1; i++) {
    assert(delta[i] >= min);
    assert(delta[i] - min <= UINT32_MAX);
    data[i] = (uint32_t)(delta[i] - min);
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
