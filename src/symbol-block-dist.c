/* This file is part of the Theseus distribution.
 * Copyright 2025 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <string.h>
#include <errno.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "translate.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "symbol-block-dist [-v] <symbol> <block size> <inputfile>\n");
  fprintf(stderr, "Create provide the distribution of the count of <symbol> in (non-overlapping) blocks in the provided file.\n");
  fprintf(stderr, "inputfile is presumed to be a stream of " STATDATA_STRING "s\n");
  fprintf(stderr, "-v\tVerbose mode (can be used several times for increased verbosity).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data = NULL;
  int opt;
  statData_t referenceSymbol;
  size_t blockSize;
  char *endptr;
  unsigned long long int inullint;
  size_t *symbolCount;

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

  if (argc != 3) {
    useageExit();
  }

  // Establish the block size
  endptr=NULL;
  inullint = strtoull(argv[1], &endptr, 0);
  if (((inullint == ULLONG_MAX) && (errno == ERANGE)) || (inullint == 0)) {
    fprintf(stderr, "Can't interpret block size value\n");
    useageExit();
  }
  blockSize = (size_t)inullint;

  // Establish the reference symbol
  endptr=NULL;
  inullint = strtoull(argv[0], &endptr, 0);
  if ((inullint >= 256) || ((inullint == 0) && (endptr == argv[0]))) {
    fprintf(stderr, "Can't interpret reference symbol value\n");
    useageExit();
  }
  referenceSymbol = (statData_t)inullint;

  if ((infp = fopen(argv[2], "rb")) == NULL) {
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

  assert(datalen >= blockSize);

  symbolCount = malloc((blockSize+1)*sizeof(size_t));

  fprintf(stderr, "Counting the number of symbol %u in %zu-symbol blocks.\n", referenceSymbol, blockSize);
  memset(symbolCount, 0, (blockSize+1)*sizeof(size_t));

  for(size_t i = 0; i < datalen; i += blockSize) {
    statData_t *curBase = data+i;
    size_t curBlockCount = 0;
    //New block processing
    for(size_t j = 0; j < blockSize; j++) {
      if(curBase[j] == referenceSymbol) curBlockCount++;
    }
    symbolCount[curBlockCount]++;
  }

  free(data);

  for(size_t j = 0; j <= blockSize; j++)
    if(symbolCount[j]>0) printf("%zu: %zu\n", j, symbolCount[j]);

  free(symbolCount);
  return EX_OK;
}
