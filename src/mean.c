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

/*Takes doubles from stdin and gives the mean*/
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "mean [-v] [-d] [-o] [-0] [-b <c>,<rounds>] [-u <low>,<high>] [filename]\n");
  fprintf(stderr, "Takes doubles from the file (if provided) or stdin (by default), 1 per line, and gives the mean.\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-d\tMake any RNG deterministic.\n");
  fprintf(stderr, "-o\tProduce only one output. If there is a confidence interval, report the minimum value.\n");
  fprintf(stderr, "-b <c>,<rounds>\tProduce <c>-BCa bootstrap confidence intervals using <rounds> of bootstrapping.\n");
  fprintf(stderr, "-u <low>,<high>\tDiscard samples that are not in the range [low, high].\n");
  fprintf(stderr, "-0\tRead in doubles in machine-specific format.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t datalen;
  char *nextOption;
  double *data;
  double indouble;
  int opt;
  double sampleMeanValue;
  double meanValueCanidate;
  bool configOneOutput;
  size_t configRounds;
  double configConfidenceInterval;
  unsigned long long int inint;
  struct randstate rstate;
  double confidenceInterval[2];
  bool configBinary;
  FILE *fp;
  double configLowBound;
  double configHighBound;

  configVerbose = 0;
  data = NULL;
  configRounds = 1;
  configConfidenceInterval = -1.0;
  configOneOutput = false;
  configBinary = false;
  configLowBound = -DBL_INFINITY;
  configHighBound = DBL_INFINITY;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "0dvob:u:")) != -1) {
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

  if ((argc != 0) && (argc != 1)) {
    useageExit();
  }

  seedGenerator(&rstate);

  if (argc == 1) {
    if ((fp = fopen(argv[0], "rb")) == NULL) {
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

  if (configConfidenceInterval >= 0.0) {
    bool validConfidenceInterval;

    sampleMeanValue = BCaBootstrapMean(data, datalen, configLowBound, configHighBound, confidenceInterval, configRounds, configConfidenceInterval, &rstate);
    if ((sampleMeanValue < configLowBound) || (sampleMeanValue > configHighBound)) {
      fprintf(stderr, "No valid data present.\n");
      exit(EX_DATAERR);
    }

    if (relEpsilonEqual(confidenceInterval[0], confidenceInterval[1], DBL_MIN, DBL_EPSILON, 4)) {
      fprintf(stderr, "No confidence interval could be produced.\n");
      validConfidenceInterval = false;
      meanValueCanidate = sampleMeanValue;
    } else {
      validConfidenceInterval = true;
      meanValueCanidate = fmin(fmin(confidenceInterval[0], sampleMeanValue), confidenceInterval[1]);
    }

    if (configOneOutput || !validConfidenceInterval) {
      printf("%.17g\n", meanValueCanidate);
    } else {
      printf("%.17g, %.17g, %.17g\n", confidenceInterval[0], sampleMeanValue, confidenceInterval[1]);
    }
  } else {
    double *trimmedData;
    // sort the data
    qsort(data, datalen, sizeof(double), doublecompare);
    // discard the invalid data
    trimmedData = data;
    datalen = trimDoubleRange(&trimmedData, datalen, configLowBound, configHighBound);
    if (configVerbose > 1) fprintf(stderr, "Sample is %zu valid samples.\n", datalen);

    sampleMeanValue = calculateMean(trimmedData, datalen);
    printf("%.17g\n", sampleMeanValue);
  }

  free(data);
  return (EX_OK);
}
