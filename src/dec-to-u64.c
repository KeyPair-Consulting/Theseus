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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include "precision.h"

/*
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "dec-to-u32\n");
  fprintf(stderr, "The values are expected to be provided via stdin, one per line.\n");
  exit(EX_USAGE);
}
*/

int main(void) {
  int res;
  uint64_t indata;

  while (feof(stdin) == 0) {
    res = scanf("%" SCNu64 "\n", &indata);
    if (res == 1) {
      if (fwrite(&indata, sizeof(uint64_t), 1, stdout) != 1) {
        perror("Can't write to stdout");
        exit(EX_OSERR);
      }
    }
  }

  return (0);
}
