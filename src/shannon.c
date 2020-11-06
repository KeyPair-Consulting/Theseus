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
  fprintf(stderr, "shannon [-v] <inputfile>\n");
  fprintf(stderr, "Calculate the shannon entropy for a data set.\n");
  fprintf(stderr, "inputfile is presumed to be a stream of " STATDATA_STRING "s\n");
  fprintf(stderr, "-v\tVerbose mode (can be used several times for increased verbosity).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data = NULL;
  double shannonEnt;
  size_t k;
  double median;
  int opt;

  configVerbose = 0;

  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
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
  if(configVerbose > 0) printf("Read in %zu integers\n", datalen);

  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  if(configVerbose > 0) fprintf(stderr, "Estimating Shannon entropy.\n");

  translate(data, datalen, &k, &median);
  shannonEnt = shannonEntropyEstimate(data, datalen, k);

  printf("Assessed Shannon entropy = %.17g\n", shannonEnt);

  free(data);

  return EX_OK;
}
