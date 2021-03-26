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
#include <sysexits.h>

#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "hweight bitmask\n");
  fprintf(stderr, "output is the hamming weight of the bitmask\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  long long int inint;
  uint32_t bitMask;
  uint32_t outputBits;

  assert(PRECISION(LLONG_MAX) > 32);

  if (argc != 2) {
    useageExit();
  }

  inint = strtoll(argv[1], NULL, 0);
  if ((inint < 0) || (inint > UINT_MAX)) {
    useageExit();
  }

  bitMask = (uint32_t)inint;
  outputBits = (uint32_t)__builtin_popcount(bitMask);

  if ((outputBits == 0) || (outputBits > 32)) {
    useageExit();
  }

  printf("%u\n", outputBits);

  return EX_OK;
}
