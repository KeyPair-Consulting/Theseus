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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-anddata [inputfile] <bitmask>\n");
  fprintf(stderr, "Takes the uint32_t symbols in (machine format) and bitwise ANDs each symbol with <bitmask>.\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "Outputs uint32_ts to stdout\n");
  fprintf(stderr, "The result is the data bitwise anded with the provided bitmask output to stdout\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t data;
  uint32_t andMask;
  long long int inll;

  if (argc == 3) {
    if ((infp = fopen(argv[1], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_NOINPUT);
    }
    argv++;
  } else if (argc == 2) {
    infp = stdin;
  } else {
    useageExit();
  }

  inll = strtoll(argv[1], NULL, 0);
  if ((inll > 0) && (inll < (int long long)UINT32_MAX)) {
    andMask = (uint32_t)inll;
  } else {
    useageExit();
  }

  fprintf(stderr, "Andmask: 0x%08x\n", andMask);

  fprintf(stderr, "Outputting data\n");

  while (feof(infp) == 0) {
    if (fread(&data, sizeof(uint32_t), 1, infp) == 1) {
      data &= andMask;

      if (fwrite(&data, sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write output to stdout");
        exit(EX_OSERR);
      }
    } else {
      perror("Can't read input from file");
      exit(EX_OSERR);
    }
  }

  fclose(infp);

  return EX_OK;
}
