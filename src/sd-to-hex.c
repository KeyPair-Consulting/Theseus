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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "entlib.h"
/*
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "sd-to-hex\n");
  fprintf(stderr, "Output the bytes in hex format.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  fprintf(stderr, "The output values are sent to stdout.\n");
  exit(EX_USAGE);
}
*/

int main(void) {
  statData_t indata;

  while (feof(stdin) == 0) {
    if (fread(&indata, sizeof(statData_t), 1, stdin) == 1) {
      printf("%X\n", indata);
    }
  }

  return (0);
}
