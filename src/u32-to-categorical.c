/* This file is part of the Theseus distribution.
 * Copyright 2020-2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>  // for getopt, optind
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>  // for uint32_t
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-to-categorical [-v] [-m] [-t <value>] <infile>\n");
  fprintf(stderr, "Produces categorical summary of data.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-m Output in Mathematica-friendly format.\n");
  fprintf(stderr, "-t <value>\tTrim any value that is prior to the first symbol with <value> occurrences or more or after the last symbol with <value> occurrences or more.\n");
  fprintf(stderr, "-s <value>\tTrim any value with fewer than <value> occurrences.\n");
  fprintf(stderr, "-z\tDon't output zero categories.\n");
  fprintf(stderr, "The values are output using stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  bool configMathematica;
  bool configOutputZeros;
  size_t configCountCutoff;
  size_t categoryCount;
  size_t *categoryTable;
  int opt;
  unsigned long long inint;
  uint32_t mindata, maxdata;
  size_t firstDataIndex, lastDataIndex;
  size_t configDisplayCutoff;

  configMathematica = false;
  configOutputZeros = true;
  configCountCutoff = 0;
  configVerbose = 0;
  configDisplayCutoff = 0;

  assert(UINT32_MAX <= SIZE_MAX);

  while ((opt = getopt(argc, argv, "zvmt:s:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'm':
        configMathematica = true;
        break;
      case 'z':
        configOutputZeros = false;
        break;
      case 's':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configDisplayCutoff = (size_t)inint;
        break;
      case 't':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configCountCutoff = (size_t)inint;
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

  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  mindata = UINT32_MAX;
  maxdata = 0;

  for (size_t i = 0; i < datalen; i++) {
    if (data[i] < mindata) mindata = data[i];
    if (data[i] > maxdata) maxdata = data[i];
  }

  assert(maxdata >= mindata);

  if (configVerbose > 0) {
    fprintf(stderr, "Minimum symbol: %u\n", mindata);
    fprintf(stderr, "Maximum symbol: %u\n", maxdata);
  }

  categoryCount = maxdata - mindata + 1;

  if ((categoryTable = calloc(categoryCount, sizeof(size_t))) == NULL) {
    free(data);
    perror("Can't allocate data for category table\n");
    exit(EX_OSERR);
  }

  for (size_t i = 0; i < datalen; i++) {
    categoryTable[data[i] - mindata]++;
  }

  firstDataIndex = categoryCount - 1;
  lastDataIndex = 0;
  for (size_t i = 0; i < categoryCount; i++) {
    if (categoryTable[i] >= configCountCutoff) {
      firstDataIndex = i;
      break;
    }
  }

  for (size_t i = categoryCount; i > 0; i--) {
    if (categoryTable[i - 1] >= configCountCutoff) {
      lastDataIndex = i - 1;
      break;
    }
  }

  if (lastDataIndex < firstDataIndex) {
    fprintf(stderr, "No data is above cutoff.");
  }

  if ((configVerbose > 0) && (configCountCutoff > 0)) {
    fprintf(stderr, "First non-discarded symbol: %u\n", (uint32_t)firstDataIndex + mindata);
    fprintf(stderr, "Last non-discarded symbol: %u\n", (uint32_t)lastDataIndex + mindata);
  }

  if (configMathematica) printf("%s={", argv[0]);

  for (size_t i = firstDataIndex; i <= lastDataIndex; i++) {
    if (configMathematica) {
      if (i < lastDataIndex) {
        if (configOutputZeros || (categoryTable[i] > configDisplayCutoff)) {
          printf("{%u, %zu},", (uint32_t)i + mindata, categoryTable[i]);
        }
      } else {
        printf("{%u, %zu}};\n", (uint32_t)i + mindata, categoryTable[i]);
      }
    } else {
      if (configOutputZeros || (categoryTable[i] > configDisplayCutoff)) {
        printf("%u: %zu\n", (uint32_t)i + mindata, categoryTable[i]);
      }
    }
  }

  free(data);
  free(categoryTable);
  return EX_OK;
}
