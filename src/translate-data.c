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
#include <getopt.h>  // for getopt, optind
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>  // for uint32_t
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <time.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "translate.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "translate-data [-v] <inputfile>\n");
  fprintf(stderr, "Perform an order-preserving map to arrange the input symbols to (0, ..., k-1)\n");
  fprintf(stderr, "inputfile is presumed to consist of " STATDATA_STRING " in machine format\n");
  fprintf(stderr, "output is sent to stdout, and is " STATDATA_STRING " in machine format\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-n\tNo data output. Report number of symbols on stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data;
  size_t k;
  int opt;
  bool configSymbolsOnly;
  double median;

  configVerbose = 0;
  configSymbolsOnly = false;
  data = NULL;

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "vn")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'n':
        configSymbolsOnly = true;
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

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  translate(data, datalen, &k, &median);
  if (configVerbose > 0) {
    fprintf(stderr, "Found %zu symbols\n", k);
  }

  if (configSymbolsOnly) {
    printf("%zu\n", k);
  } else {
    if (fwrite(data, sizeof(statData_t), datalen, stdout) != datalen) {
      perror("Can't write to output");
      exit(EX_OSERR);
    }
  }

  free(data);
  return EX_OK;
}
