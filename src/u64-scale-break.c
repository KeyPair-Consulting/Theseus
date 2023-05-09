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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u64-scale-break <break> <scaleHigh> <scaleLow>\n");
  fprintf(stderr, "<break>\tNumber of bits in low order.\n");
  fprintf(stderr, "<scaleHigh>\tFactor to multiply high value.\n");
  fprintf(stderr, "<scaleLow>\tFactor to multiply low value.\n");
  fprintf(stderr, "The values are expected to be provided via stdin and the output via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  uint16_t configLowBits=10;
  uint64_t configScaleHigh;
  uint64_t configScaleLow;
  char *endptr;
  unsigned long int intval;
  uint64_t bitmask;

  if (argc != 4) {
    useageExit();
  }

  intval = strtoul(argv[1], &endptr, 0);
  if((*endptr != '\0') || (intval > 63) || (intval == 0))
    useageExit();
  configLowBits = (uint16_t)intval;

  intval = strtoul(argv[2], &endptr, 0);
  if((*endptr != '\0') || (intval == ULONG_MAX))
    useageExit();
  configScaleHigh = intval;

  intval = strtoul(argv[3], &endptr, 0);
  if((*endptr != '\0') || (intval == ULONG_MAX))
    useageExit();
  configScaleLow = intval;

  bitmask = (1ULL << configLowBits) - 1ULL;
  fprintf(stderr, "width: %u, scaleHigh: %zu, scaleLow: %zu, bitmask: 0x%016lX\n", configLowBits, configScaleHigh, configScaleLow, bitmask);

  while (feof(stdin) == 0) {
    uint64_t inData;
    size_t res;
    uint64_t curVal;
    res = fread(&inData, sizeof(uint64_t), 1, stdin);
    if (res == 1) {
      curVal = ((inData & bitmask) * configScaleLow) + ((inData >> configLowBits) * configScaleHigh);

      if (fwrite(&curVal, sizeof(uint64_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }
  return (0);
}
