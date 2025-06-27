/* This file is part of the Theseus distribution.
 * Copyright 2024 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <getopt.h>  // for getopt, optind
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u64-to-u32 [-r] [-t]\n");
  fprintf(stderr, "-r\tSwitch endianness of input values.\n");
  fprintf(stderr, "-t\tTruncate input values.\n");
  fprintf(stderr, "The values are expected to be provided via stdin and the output via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t res;
  uint64_t inData;
  uint32_t out;
  bool configReverse;
  bool configTruncate;
  int opt;

  assert(PRECISION(UINT_MAX) == 32);

  configReverse = false;
  configTruncate = false;

  while ((opt = getopt(argc, argv, "rt")) != -1) {
    switch (opt) {
      case 'r':
        configReverse = true;
        break;
      case 't':
        configTruncate = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  if (argc != optind) {
    useageExit();
  }

  while (feof(stdin) == 0) {
    res = fread(&inData, sizeof(uint64_t), 1, stdin);
    if (res == 1) {
      if (configReverse) {
        inData = reverse64(inData);
      }

      if (configTruncate) inData = inData & 0xFFFFFFFF;

      if (inData > UINT32_MAX) {
        fprintf(stderr, "raw input out of range: %016" PRIx64 "\n", inData);
        exit(EX_DATAERR);
      } else {
        out = (uint32_t)inData;
      }

      if (fwrite(&out, sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
