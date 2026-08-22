/* This file is part of the Theseus distribution.
 * Copyright 2026 Joshua E. Hill <josh@keypair.us>
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
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-u32-jent-nonstuck <infile>\n");
  fprintf(stderr, "Filters data to output only elements that are non-stuck. Reports statistics.\n");
  fprintf(stderr, "-d output differences between adjacent values to find raw data.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t *filteredData = NULL;
  bool configDiffMode;
  size_t discardedSamples = 0;
  size_t delta1 = 0, delta2=0, delta3=0, runCount=0;
  size_t filteredDataLen = 0;
  int opt;

  configDiffMode = false;

  while ((opt = getopt(argc, argv, "d")) != -1) {
    switch (opt) {
      case 'd':
        configDiffMode = true;
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

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);

  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  if (configDiffMode) {
    for (size_t i = 1; i < datalen; i++) {
      data[i - 1] = data[i] - data[i - 1];
    }
    datalen--;
  }

  if((filteredData = malloc(sizeof(uint32_t) * datalen))==NULL) {
    perror("Can't allocate data for output.");
    exit(EX_OSERR);
  }

  for(size_t i=0; i < datalen; i++) {
    bool shouldOutputData = true;
    if(data[i] == 0) {
      shouldOutputData = false;
      delta1++;
    }
    if((i>0)) {
      if(data[i] == data[i-1]) {
        shouldOutputData = false;
        delta2++;
      } 
    }

    if((i>1) && (data[i] == 2*data[i-1] - data[i-2])) {
      shouldOutputData = false;
      delta3++;
      if((data[i] == data[i-1]) && (data[i] == data[i-2])) {
        runCount++;
      }
    }

    if(shouldOutputData) {
      filteredData[filteredDataLen] = data[i];
      filteredDataLen++;
    }
  } 

  if (fwrite(filteredData, sizeof(uint32_t), filteredDataLen, stdout) != filteredDataLen) {
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  discardedSamples = datalen - filteredDataLen;
  
  fprintf(stderr, "Filtered %zu symbols (cond 1 = %zu, cond 2 = %zu, cond 3 = %zu, cond2 AND cond3 = %zu).\n", discardedSamples, delta1, delta2, delta3, runCount);
  fprintf(stderr, "m=%.17g\n", (double)discardedSamples/(double)datalen);
  free(data);
  free(filteredData);

  return (0);
}
