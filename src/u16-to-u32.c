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


#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u16-to-u32 [-d]\n");
  fprintf(stderr, "-d output differences between adjacent values.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t res;
  bool configDiffMode;
  uint16_t lastSymbol;
  int opt;

  assert(PRECISION(UINT_MAX) == 32);

  configDiffMode = false;

  while ((opt = getopt(argc, argv, "d")) != -1) {
    switch (opt) {
      case 'd':
        configDiffMode = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  if (argc != optind) {
    useageExit();
  }

  if (configDiffMode) {
    res = fread(&lastSymbol, sizeof(uint16_t), 1, stdin);
    if (res != 1) {
      perror("Can't read initial symbol");
      exit(EX_OSERR);
    }
  } else {
    lastSymbol = 0;
  }

  while (feof(stdin) == 0) {
    uint32_t outdata;
    uint16_t indata;

    res = fread(&indata, sizeof(uint16_t), 1, stdin);
    if (res == 1) {
      if (configDiffMode) {
        uint16_t curdelta;
        curdelta = (uint16_t)(indata - lastSymbol);
        outdata = (uint32_t)curdelta;
        lastSymbol = indata;
      } else {
        outdata = (uint32_t)indata;
      }

      if (fwrite(&outdata, sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
