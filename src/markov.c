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
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "translate.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "markov [-v] [-c] [-p <cutoff>] <inputfile>\n");
  fprintf(stderr, "Run some variant of the SP 800-90B 2016 Markov test.\n");
  fprintf(stderr, "inputfile is presumed to be a stream of " STATDATA_STRING "s\n");
  fprintf(stderr, "-v\tVerbose mode (can be used several times for increased verbosity).\n");
  fprintf(stderr, "-p <cutoff>\tThe lowest acceptable probablility for a symbol to be relevant.\n");
  fprintf(stderr, "-c\tDisable the creation of confidence intervals.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data = NULL;
  double minent;
  size_t k;
  double median;
  int opt;
  bool configConfidenceIntervals;
  double configProbCutoff = 0.0;
  double indouble;

  configVerbose = 0;
  configConfidenceIntervals = true;
  configProbCutoff = 0.0;

  while ((opt = getopt(argc, argv, "vcp:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'c':
        configConfidenceIntervals = false;
        break;
      case 'p':
        indouble = strtod(optarg, NULL);
        if ((indouble > 1.0) || (indouble < 0.0)) {
          useageExit();
        }

        configProbCutoff = indouble;
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

  datalen = readuintfile(infp, &data);
  assert(data != NULL);
  printf("Read in %zu integers\n", datalen);
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  fprintf(stderr, "Running markov test.\n");

  translate(data, datalen, &k, &median);
  minent = NSAMarkovEstimate(data, datalen, k, "Literal", configConfidenceIntervals, configProbCutoff);

  printf("Assessed min entropy = %.17g\n", minent);

  free(data);

  return EX_OK;
}
