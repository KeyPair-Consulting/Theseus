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
#include <string.h>
#include <sysexits.h>
#include <getopt.h>

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-counter-endian [-d] <infile>\n");
  fprintf(stderr, "Trys to guess counter endianness, and translates to the local platform.\n");
  fprintf(stderr, "-d output differences between adjacent values (when viewing the data as a 32-bit unsigned counter with rollover).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  size_t i;
  size_t incCountNative, incCountReversed;
  bool configDiffMode;
  int opt;

  configDiffMode = false;
  incCountNative = 0;
  incCountReversed = 0;

  while ((opt = getopt(argc, argv, "d")) != -1) {
    switch (opt) {
      case 'd':
        configDiffMode = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    useageExit();
  }

  if ((infp = fopen(argv[0], "rb")) == NULL) {
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

  for (i = 1; i < datalen; i++) {
    if (data[i - 1] <= data[i]) incCountNative++;
    if (reverse32(data[i - 1]) <= reverse32(data[i])) incCountReversed++;
  }

  if (incCountNative >= incCountReversed) {
    fprintf(stderr, "Native format detected (%.17g vs %.17g)\n", ((double)incCountNative) / ((double)datalen), ((double)incCountReversed) / ((double)datalen));
  } else {
    fprintf(stderr, "Reversed format detected (%.17g vs %.17g)\n", ((double)incCountReversed) / ((double)datalen), ((double)incCountNative) / ((double)datalen));
    for (i = 0; i < datalen; i++) {
      data[i] = reverse32(data[i]);
    }
  }

  if (configDiffMode) {
    for (i = 1; i < datalen; i++) {
      data[i - 1] = data[i] - data[i - 1];
    }
    datalen--;
  }

  if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  free(data);

  return (0);
}
