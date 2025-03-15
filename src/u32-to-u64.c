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
  fprintf(stderr, "u32-to-u64 [-r] [-t]\n");
  fprintf(stderr, "-r\tSwitch endianness of input values.\n");
  fprintf(stderr, "The values are expected to be provided via stdin and the output via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t res;
  uint32_t inData;
  uint64_t out;
  bool configReverse;
  int opt;

  configReverse = false;

  while ((opt = getopt(argc, argv, "r")) != -1) {
    switch (opt) {
      case 'r':
        configReverse = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  if (argc != optind) {
    useageExit();
  }

  while (feof(stdin) == 0) {
    res = fread(&inData, sizeof(uint32_t), 1, stdin);

    if (res == 1) {
      out = (uint64_t)inData;
      if (configReverse) {
        out = reverse64(out);
      }

      if (fwrite(&out, sizeof(uint64_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
