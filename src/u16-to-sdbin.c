/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "entlib.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u16-to-sd [-l] [-b]\n");
  fprintf(stderr, "Expand packed bits that are stored in u16 values.\n");
  fprintf(stderr, "-l\t extract bits from low bit to high bit\n");
  fprintf(stderr, "-b\t 16 bit values are in big endian format.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  statData_t outdata;
  uint16_t indata;
  uint16_t bitmask;
  size_t j;
  bool configLTH;
  bool configBigEndian;
  int opt;

  configLTH = false;
  configBigEndian = false;

  while ((opt = getopt(argc, argv, "lb")) != -1) {
    switch (opt) {
      case 'l':
        configLTH = true;
        break;
      case 'b':
        configBigEndian = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;

  if (argc != 0) {
    useageExit();
  }

  while (feof(stdin) == 0) {
    if (fread(&indata, sizeof(uint16_t), 1, stdin) == 1) {
      if (configBigEndian) {
        indata = (uint16_t)(((indata >> 8) & 0x00FF) | ((indata << 8) & 0xFF00));
      }
      if (configLTH) {
        bitmask = (uint16_t)0x0001;
      } else {
        bitmask = (uint16_t)0x8000;
      }

      for (j = 0; j < 16; j++) {
        outdata = (statData_t)(((indata & bitmask) == 0) ? 0U : 1U);
        if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
          perror("Can't write to output");
          exit(EX_OSERR);
        }

        if (configLTH) {
          bitmask = (uint16_t)((bitmask << 1) & 0xFFFF);
        } else {
          bitmask = bitmask >> 1;
        }
      }
    }
  }

  return (0);
}
