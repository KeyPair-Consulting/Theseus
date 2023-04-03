/* This file is part of the Theseus distribution.
 * Copyright 2023 Joshua E. Hill <josh@keypair.us>
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
  fprintf(stderr, "interleave-data <inputfile1> <inputfile2>\n");
  fprintf(stderr, "Interleave the data from inputfile1 and inputfile2\n");
  fprintf(stderr, "files are presumed to consist of " STATDATA_STRING " in machine format\n");
  fprintf(stderr, "output is sent to stdout, and is " STATDATA_STRING " in machine format\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen1, datalen2;
  statData_t *data1;
  statData_t *data2;
  statData_t *combinedData;
  size_t symbolSets;
  int opt;

  configVerbose = 0;
  data1 = NULL;
  data2 = NULL;

  assert(PRECISION(UINT_MAX) >= 32);

  while ((opt = getopt(argc, argv, "vn")) != -1) {
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

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open inputfile1");
    exit(EX_NOINPUT);
  }

  datalen1 = readuintfile(infp, &data1);
  assert(data1 != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers from inputfile1\n", datalen1);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close inputfile1");
    exit(EX_OSERR);
  }

  assert(datalen1 > 0);

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open inputfile2");
    exit(EX_NOINPUT);
  }

  datalen2 = readuintfile(infp, &data2);
  assert(data2 != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers from inputfile2\n", datalen2);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close inputfile2");
    exit(EX_OSERR);
  }

  assert(datalen2 > 0);

  symbolSets = (datalen1<datalen2)?datalen1:datalen2;

  if((combinedData = malloc(sizeof(statData_t)*symbolSets*2))==NULL) {
    perror("Couldn't allocate memory");
    exit(EX_OSERR);
  }

  for(size_t i=0; i<symbolSets; i++) {
    combinedData[2*i] = data1[i];
    combinedData[2*i+1] = data2[i];
  }

  if (fwrite(combinedData, sizeof(statData_t), symbolSets*2, stdout) != symbolSets*2) {
    perror("Can't write to output");
    exit(EX_OSERR);
  }

  free(data1);
  free(data2);
  free(combinedData);
  return EX_OK;
}
