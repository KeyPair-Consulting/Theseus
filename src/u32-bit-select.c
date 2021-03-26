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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binutil.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-bit-select [-r] (bit)\n");
  fprintf(stderr, "-r\tReverse the endianness of the u32 inputs before selecting the bit.\n");
  fprintf(stderr, "Outputs only the selected bit (0 is the lsb, 31 is msb).\n");
  fprintf(stderr, "The 32-bit values are expected to be provided via stdin.\n");
  fprintf(stderr, "output is " STATDATA_STRING " via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  uint32_t data;
  uint32_t curbitmask;
  int bitpos;
  int opt;
  bool configReverse;

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

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    useageExit();
  } else {
    bitpos = atoi(argv[0]);
  }

  if ((bitpos < 0) || (bitpos > 31)) {
    useageExit();
  }

  curbitmask = 1U << bitpos;

  if (configReverse) {
    curbitmask = reverse32(curbitmask);
  }

  while (feof(stdin) == 0) {
    size_t res;
    statData_t outdata;

    res = fread(&data, sizeof(uint32_t), 1, stdin);
    if (res == 1) {
      outdata = (statData_t)(((curbitmask & data) == 0) ? 0U : 1U);
      if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
