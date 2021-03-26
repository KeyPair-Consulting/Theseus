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

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u8-to-sd [-l] [-v] <bits per symbol; 1, 2, 4>\n");
  fprintf(stderr, "-l\tBytes should be output low bits to high bits.\n");
  fprintf(stderr, "-v\tIncrease verbosity. Can be used multiple times.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  fprintf(stderr, "The output values are sent to stdout in the form of " STATDATA_STRING " integers.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  statData_t outdata;
  uint8_t indata;
  uint8_t bitmask;
  uint8_t configBitsPerSample;
  uint8_t j;
  bool configLTH;
  int configVerbose;
  int opt;
  unsigned long inint;

  configLTH = false;
  configVerbose = 0;

  while ((opt = getopt(argc, argv, "vl")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'l':
        configLTH = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    useageExit();
  }

  inint = strtoul(argv[0], NULL, 0);
  if ((inint == ULONG_MAX) || (errno == EINVAL) || (inint > 4)) {
    useageExit();
  }

  configBitsPerSample = (uint8_t)inint;
  if ((configBitsPerSample != 1) && (configBitsPerSample != 2) && (configBitsPerSample != 4)) {
    if (configVerbose > 0) fprintf(stderr, "Bits per sample: %u\n", configBitsPerSample);
    useageExit();
  }

  while (feof(stdin) == 0) {
    if (fread(&indata, sizeof(uint8_t), 1, stdin) == 1) {
      if (configVerbose > 1) fprintf(stderr, "indata: 0x%X\n", indata);

      // Set the low order configBitsPerSample bits in the bitmask
      bitmask = (uint8_t)((1U << configBitsPerSample) - 1U);
      if (!configLTH) {
        // Shift the masked bits to the high order
        bitmask = (uint8_t)(bitmask << (8U - configBitsPerSample));
      }

      for (j = 0; j < 8; j = (uint8_t)(j + configBitsPerSample)) {
        if (configVerbose > 1) fprintf(stderr, "Bitmask: 0x%X\n", bitmask);
        assert(bitmask != 0);
        outdata = (statData_t)(indata & bitmask);
        if (configVerbose > 1) fprintf(stderr, "pre-outdata: 0x%X\n", outdata);
        if (configLTH) {
          outdata = (uint8_t)(outdata >> j);
        } else {
          outdata = (uint8_t)(outdata >> (8U - configBitsPerSample - j));
        }

        if (configVerbose > 1) fprintf(stderr, "outdata: 0x%X\n", outdata);
        if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
          perror("Can't write to output");
          exit(EX_OSERR);
        }

        if (configLTH) {
          bitmask = (uint8_t)(bitmask << configBitsPerSample);
        } else {
          bitmask = (uint8_t)(bitmask >> configBitsPerSample);
        }
      }
    }
  }

  return (0);
}
