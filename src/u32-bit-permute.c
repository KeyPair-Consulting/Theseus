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
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "binutil.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-bit-permute [-r] <bit specification>\n");
  fprintf(stderr, "-r\tReverse the endianness of the u32 inputs before permuting.\n");
  fprintf(stderr, "Bit ordering is in the LSB 0 format (that is, bit 0 is the LSB, bit 31 is the MSB)\n");
  fprintf(stderr, "Ordering of the bit specification is left to right, MSB to LSB, so the specification \"31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0\" is the identity permutation.\n");
  fprintf(stderr, "If fewer than 32 output bits are within the specification, the unspecified high order bits are set to 0\n");
  fprintf(stderr, "Each bit position can be present at most once.\n");
  fprintf(stderr, "The 32-bit values are expected to be provided via stdin.\n");
  fprintf(stderr, "output is u32 values via stdout in machine native format.\n");
  fprintf(stderr, "Example: u32-bit-permute \n");
  exit(EX_USAGE);
}

static void strtoindexarray(const char *nptr, uint8_t *bitpos) {
  long int inint;
  char *endptr = NULL;
  uint8_t bitposIndex[32];
  uint8_t paramIndex = 0;

  memset(bitposIndex, 32, sizeof(bitposIndex));

  assert(nptr != NULL);

  while (*nptr != '\0') {
    uint8_t curIndex;

    if (paramIndex >= 32) {
      fprintf(stderr, "Too many parameters provided.\n");
      useageExit();
    }

    inint = strtol(nptr, &endptr, 0);
    assert(endptr != NULL);
    if (endptr != nptr) {
      if ((inint < 0L) || (inint > 31)) {
        fprintf(stderr, "Can't convert integer: out of range\n");
        exit(EX_USAGE);
      }
      curIndex = (uint8_t)inint;

      if (bitposIndex[curIndex] == 32U) {
        bitposIndex[curIndex] = paramIndex;
        bitpos[paramIndex] = curIndex;
        paramIndex++;
      } else {
        fprintf(stderr, "Bit %u specified more than once (that index is presently %u).\n", curIndex, bitposIndex[curIndex]);
        useageExit();
      }

      if (*endptr == ':') {
        endptr++;
      }
      nptr = endptr;
    } else {
      fprintf(stderr, "Found non-integer input.\n");
      useageExit();
    }
  }

  fprintf(stderr, "Keeping %u bits per input symbol.\n", paramIndex);
}

int main(int argc, char *argv[]) {
  uint32_t data;
  uint8_t outputBitpos[33];  // msb to lsb
  int opt;
  bool configReverse;

  memset(outputBitpos, 32, sizeof(outputBitpos));
  configReverse = false;

  while ((opt = getopt(argc, argv, "r")) != -1) {
    switch (opt) {
      case 'r':
        configReverse = true;
        break;
      default: /* ? */
        fprintf(stderr, "Invalid parameter: '%c'.\n", opt);
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    fprintf(stderr, "Wrong number of parameters.\n");
    useageExit();
  } else {
    strtoindexarray(argv[0], outputBitpos);
  }

  while (feof(stdin) == 0) {
    uint32_t curOutput = 0;
    size_t res;

    if (fread(&data, sizeof(uint32_t), 1, stdin) == 1) {
      uint8_t curOutputIndex = 0;
      if (configReverse) {
        data = reverse32(data);
      }

      while (outputBitpos[curOutputIndex] < 32) {
        curOutput = curOutput << 1;
        curOutput = curOutput | ((data >> outputBitpos[curOutputIndex]) & 1U);
        curOutputIndex++;
      }

      res = fwrite(&curOutput, sizeof(uint32_t), 1, stdout);
      assert(res == 1);
    }
  }

  return (0);
}
