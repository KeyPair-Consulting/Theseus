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

int main(void) {
  statData_t indata;

  while (feof(stdin) == 0) {
    if (fread(&indata, sizeof(statData_t), 1, stdin) == 1) {
      printf("%u\n", indata);
    }
  }

  return (0);
}
