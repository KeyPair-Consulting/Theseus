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
#include <stdnoreturn.h>
#include "precision.h"

/*noreturn static void useageExit(void)
{
   fprintf(stderr, "Usage:\n");
   fprintf(stderr, "u32-to-ascii\n");
   fprintf(stderr, "The values are expected to be provided via stdin.\n");
   exit(EX_USAGE);
}*/

int main(void) {
  uint64_t data;

  assert(PRECISION(UINT_MAX) == 32);

  while (feof(stdin) == 0) {
    if (fread(&data, sizeof(uint64_t), 1, stdin) == 1) {
      printf("%zu\n", data);
    }
  }

  return (0);
}
