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
  fprintf(stderr, "double-minmaxdelta [-v] [-0] [filename]\n");
  fprintf(stderr, "Takes doubles from the file (if provided) or stdin (by default) (1 per line in ascii mode) and gives the mean.\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-0\tRead in doubles in machine-specific format.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t datalen;
  double *data;
  int opt;
  bool configBinary;
  FILE *fp;
  double minValue=DBL_INFINITY, maxValue=-DBL_INFINITY;

  configVerbose = 0;
  data = NULL;
  configBinary = false;

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "0v")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
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

  if ((argc != 0) && (argc != 1)) {
    useageExit();
  }

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

  for(size_t j=0; j<datalen; j++) {
    if(data[j] > maxValue) maxValue = data[j];
    if(data[j] < minValue) minValue = data[j];
  }

  if(configVerbose > 0) {
    fprintf(stderr, "Max: %.17g\n", maxValue);
    fprintf(stderr, "Min: %.17g\n", minValue);
  }

  printf("%.17g\n", maxValue-minValue);

  free(data);
  return (EX_OK);
}
