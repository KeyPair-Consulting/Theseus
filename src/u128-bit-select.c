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
  fprintf(stderr, "bit-select-u128 (bit)\n");
  fprintf(stderr, "The 128-bit values are expected to be provided via stdin.\n");
  fprintf(stderr, "output is " STATDATA_STRING " via stdout.\n");
  exit(EX_USAGE);
}

/*0 - 63 in the first uint64
  64 - 127 in the second uint64
  (assume little endian)*/
static void bitmask(uint64_t *input, int bitpos) {
  assert(input != NULL);
  assert((bitpos >= 0) && (bitpos <= 127));

  if (bitpos >= 64) {
    input[0] = 0;
    input[1] = 0x01ULL << (bitpos - 64);
  } else {
    input[0] = 0x01ULL << (bitpos);
    input[1] = 0;
  }

  assert((__builtin_popcountll(input[0]) + __builtin_popcountll(input[1])) == 1);
}

int main(int argc, char *argv[]) {
  uint64_t data[2];
  statData_t outdata;
  uint64_t curbitmask[2];
  size_t res;
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

  if ((bitpos < 0) || (bitpos > 127)) {
    useageExit();
  }

  bitmask(curbitmask, bitpos);
  if (configReverse) {
    reverse128(curbitmask);
  }

  while (feof(stdin) == 0) {
    res = fread(data, sizeof(uint64_t), 2, stdin);
    if (res == 2) {
      outdata = (((curbitmask[0] & data[0]) | (curbitmask[1] & data[1])) == 0) ? 0 : 1;
      if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
