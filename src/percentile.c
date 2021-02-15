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

/*Takes doubles from stdin (1 per line) and gives the pth percentile*/
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "percentile [-l] [-v] [-d] [-o] [-0] [-b <low>,<high>] <p> [filename]\n");
  fprintf(stderr, "Takes doubles from the file (if provided) or stdin (by default), 1 per line, and gives the pth percentile\n");
  fprintf(stderr, "percentile estimated using the recommended NIST method (Hyndman and Fan's R6).\n");
  fprintf(stderr, "See: http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm\n");
  fprintf(stderr, "data is presumed to be passed in via stdin, consisting of a series of lines of doubles.\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-d\tMake any RNG deterministic.\n");
  fprintf(stderr, "-o\tProduce only one output. If there is a confidence interval, report the minimum value.\n");
  fprintf(stderr, "-l\tTreat the last value as an upper bound, rather than a data element. Report a single value, the min of the upper bound and the assessed value or smallest value in the confidence interval.\n");
  fprintf(stderr, "-b <c>,<rounds>\tProduce <c>-BCa bootstrap confidence intervals using <rounds> of bootstrapping.\n");
  fprintf(stderr, "-u <low>,<high>\tDiscard samples that are not in the range [low, high].\n");
  fprintf(stderr, "-0\tRead in doubles in machine-specific format.\n");
  fprintf(stderr, "<p>\tReturn the pth percentile p in [0, 1]\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t datalen;
  char *nextOption;
  double *data;
  double indouble;
  int opt;
  double configPercentile;
  double percentileValueCanidate;
  double samplePercentileValue;
  bool configLastMin;
  bool configOneOutput;
  size_t configRounds;
  double lastValue = -DBL_INFINITY;
  double configConfidenceInterval;
  unsigned long long int inint;
  struct randstate rstate;
  double confidenceInterval[2];
  bool configBinary;
  FILE *fp;
  double configLowBound;
  double configHighBound;

  configVerbose = 0;
  configLastMin = false;
  data = NULL;
  configRounds = 1;
  configConfidenceInterval = -1.0;
  configOneOutput = false;
  configBinary = false;
  configLowBound = 0.0;
  configHighBound = 8.0;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "0dvlob:u:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'o':
        configOneOutput = true;
        break;
      case 'd':
        rstate.deterministic = true;
        break;
      case 'l':
        configLastMin = true;
        break;
      case '0':
        configBinary = true;
        break;
      case 'b':
        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (indouble > 1.0) || (*nextOption != ',')) {
          useageExit();
        }
        configConfidenceInterval = indouble;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configRounds = inint;
        break;
      case 'u':
        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (errno == ERANGE) || (*nextOption != ',')) {
          useageExit();
        }
        configLowBound = indouble;

        nextOption++;

        indouble = strtod(nextOption, NULL);
        if (errno == ERANGE) {
          useageExit();
        }
        configHighBound = indouble;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if ((argc != 1) && (argc != 2)) {
    useageExit();
  }

  seedGenerator(&rstate);

  // What is the target percentile?
  indouble = strtod(argv[0], NULL);
  if ((indouble > 1.0) || (indouble < 0.0) || (errno == ERANGE)) {
    useageExit();
  }
  configPercentile = indouble;

  if (argc == 2) {
    if ((fp = fopen(argv[1], "rb")) == NULL) {
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

  // Is the last element in the list really data?
  if (configLastMin) {
    lastValue = data[datalen - 1];
    datalen--;
    if (configVerbose > 0) fprintf(stderr, "Last element (upper bound): %.17g\n", lastValue);
  }

  assert(data != NULL);

  if (configConfidenceInterval >= 0.0) {
    bool validConfidenceInterval;

    samplePercentileValue = BCaBootstrapPercentile(configPercentile, data, datalen, configLowBound, configHighBound, confidenceInterval, configRounds, configConfidenceInterval, &rstate);
    if ((samplePercentileValue < configLowBound) || (samplePercentileValue > configHighBound)) {
      fprintf(stderr, "No valid data present.\n");
      exit(EX_DATAERR);
    }

    if (relEpsilonEqual(confidenceInterval[0], confidenceInterval[1], DBL_MIN, DBL_EPSILON, 4)) {
      fprintf(stderr, "No confidence interval could be produced.\n");
      validConfidenceInterval = false;
      percentileValueCanidate = samplePercentileValue;
    } else {
      validConfidenceInterval = true;
      percentileValueCanidate = fmin(fmin(confidenceInterval[0], samplePercentileValue), confidenceInterval[1]);
    }

    if (configLastMin) {
      if (lastValue < percentileValueCanidate) {
        if (configVerbose > 0) fprintf(stderr, "Output value restricted by provided maximum.\n");
        percentileValueCanidate = lastValue;
      }
      printf("%.17g\n", percentileValueCanidate);
    } else if (configOneOutput || !validConfidenceInterval) {
      printf("%.17g\n", percentileValueCanidate);
    } else {
      printf("%.17g, %.17g, %.17g\n", confidenceInterval[0], samplePercentileValue, confidenceInterval[1]);
    }
  } else {
    samplePercentileValue = calculatePercentile(configPercentile, data, datalen, configLowBound, configHighBound);
    if (configLastMin && (lastValue < samplePercentileValue)) {
      if (configVerbose > 0) fprintf(stderr, "Output value restricted by provided maximum.\n");
      printf("%.17g\n", lastValue);
    } else {
      printf("%.17g\n", samplePercentileValue);
    }
  }

  free(data);
  return (EX_OK);
}
