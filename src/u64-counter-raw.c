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
#include <inttypes.h>
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

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u64-counter-raw <filename>\n");
  fprintf(stderr, "Extract deltas treated as 64-bit unsigned counters (that roll may roll over).\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint64_t *data = NULL;
  size_t i;
  uint64_t delta;
  uint64_t mindelta = UINT64_MAX;
  uint64_t maxdelta = 0;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint64file(infp, &data);
  assert(data != NULL);

  fprintf(stderr, "Read in %" PRId64 " uint64_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  if (datalen < 2) {
    fprintf(stderr, "Too little data\n");
    exit(EX_DATAERR);
  }

  for (i = 1; i < datalen; i++) {
    delta = data[i] - data[i - 1];
    if (delta < mindelta) mindelta = delta;
    if (delta > maxdelta) maxdelta = delta;
    data[i - 1] = delta;
  }
  if (mindelta != 0) {
    fprintf(stderr, "Shifting data down by %" PRIu64 ". Maximum value now %" PRIu64 "\n", mindelta, maxdelta - mindelta);

    for (i = 0; i < datalen - 1; i++) {
      data[i] -= mindelta;
    }
  }

  if (fwrite(data, sizeof(uint64_t), datalen - 1, stdout) != datalen - 1) {
    if (data != NULL) free(data);
    data = NULL;
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  if (data != NULL) free(data);
  data = NULL;
  return (0);
}
