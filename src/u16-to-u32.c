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

#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u16-to-u32 [-d]\n");
  fprintf(stderr, "-d output differences between adjacent values.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  uint32_t data;
  size_t res;
  uint16_t indata;
  bool diffMode;
  uint16_t lastSymbol;
  uint32_t delta;

  assert(PRECISION(UINT_MAX) == 32);

  diffMode = false;

  if (argc == 2) {
    if (strncmp(argv[1], "-d", 2) == 0) {
      diffMode = true;
    } else {
      useageExit();
    }
  } else if (argc > 2) {
    useageExit();
  }

  if (diffMode) {
    res = fread(&indata, sizeof(uint16_t), 1, stdin);
    if (res == 1) {
      lastSymbol = indata;
    } else {
      perror("Can't read initial symbol");
      exit(EX_OSERR);
    }
  } else {
    lastSymbol = 0;
  }

  while (feof(stdin) == 0) {
    res = fread(&indata, sizeof(uint16_t), 1, stdin);
    if (res == 1) {
      if (diffMode) {
        delta = (uint32_t)(indata - lastSymbol);

        if (fwrite(&delta, sizeof(uint32_t), 1, stdout) != 1) {
          perror("Can't write to stdout");
          exit(EX_OSERR);
        }
        lastSymbol = indata;
      } else {
        data = (uint32_t)indata;
        if (fwrite(&data, sizeof(uint32_t), 1, stdout) != 1) {
          perror("Can't write to stdout");
          exit(EX_OSERR);
        }
      }
    }
  }

  return (0);
}
