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
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "bootstrap.h"
#include "fancymath.h"
#include "globals-inst.h"
#include "precision.h"
#include "randlib.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "failrate [-v] [-d] [-0] <p> <q> [filename]\n");
  fprintf(stderr, "Takes doubles from the file (if provided) or stdin (by default), 1 per line or in binary format, and gives the proportion that are less than or equal to p or greater than or equal to q.\n");
  fprintf(stderr, "Useful to characterize false positive rates for statistical tests with inclusive failure bounds.\n");
  fprintf(stderr, "data is presumed to be passed in via stdin, consisting of a series of lines of doubles.\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-s\tAssume that the data is sorted.\n");
  fprintf(stderr, "-0\tRead in doubles in machine-specific format.\n");
  fprintf(stderr, "<p>\tLower bound\n");
  fprintf(stderr, "<q>\tUpper bound\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t datalen;
  double *data;
  double indouble;
  int opt;
  bool configBinary;
  FILE *fp;
  double configLowBound;
  double configHighBound;
  size_t lowFails;
  size_t highFails;
  bool configSorted;

  configVerbose = 0;
  data = NULL;
  configBinary = false;
  configSorted = true;

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "0sv")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 's':
        configSorted = true;
        break;
      case '0':
        configBinary = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if ((argc != 2) && (argc != 3)) {
    useageExit();
  }

  // What is the target lower bound?
  indouble = strtod(argv[0], NULL);
  configLowBound = indouble;

  // What is the target upper bound?
  indouble = strtod(argv[1], NULL);
  configHighBound = indouble;

  if (argc == 3) {
    if ((fp = fopen(argv[2], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_OSERR);
    }
  } else {
    fp = stdin;
  }

  if (configBinary) {
    datalen = readdoublefile(fp, &data);
  } else {
    datalen = readasciidoubles(fp, &data);
  }

  if (datalen == 0) {
    useageExit();
  }

  assert(data != NULL);

  if (!configSorted) {
    // Sort the data
    qsort(data, datalen, sizeof(double), doublecompare);
  }

  {
    size_t numUnder, numOver, numEqual;
    numUnder = belowValue(configLowBound, data, datalen);
    numOver = aboveValue(configLowBound, data, datalen);
    assert(numUnder + numOver <= datalen);
    numEqual = datalen - numUnder - numOver;

    lowFails = numUnder + numEqual;
    fprintf(stderr, "Proportion in lower failure region: %.17g\n", ((double)lowFails) / ((double)datalen));
  }

  {
    size_t numUnder, numOver, numEqual;
    numUnder = belowValue(configHighBound, data, datalen);
    numOver = aboveValue(configHighBound, data, datalen);
    assert(numUnder + numOver <= datalen);
    numEqual = datalen - numUnder - numOver;

    highFails = numOver + numEqual;
    fprintf(stderr, "Proportion in upper failure region: %.17g\n", ((double)highFails) / ((double)datalen));
  }

  printf("Proportion in failure region: %.17g\n", (double)(highFails + lowFails) / (double)datalen);

  free(data);
  return (EX_OK);
}
