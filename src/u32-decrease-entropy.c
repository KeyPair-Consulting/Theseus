/* This file is part of the Theseus distribution.
 * Copyright 2024 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <math.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "fancymath.h"
#include "randlib.h"

static int uintcompare(const void *in1, const void *in2) {
  const uint32_t *left;
  const uint32_t *right;

  left = in1;
  right = in2;

  if (*left < *right) {
    return (-1);
  } else if (*left > *right) {
    return (1);
  } else {
    return (0);
  }
}

/*This is the Fisher Yates shuffle*/
/*See https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle*/
static void FYShuffle64(size_t *data, size_t datalen, struct randstate *rstate) {
  assert(datalen > 0);
  assert(datalen <= UINT64_MAX);

  for (size_t i = datalen - 1; i >= 1; i--) {
    size_t tmp;
    size_t j = randomRange64((uint64_t)i, rstate);
    tmp = data[i];
    data[i] = data[j];
    data[j] = tmp;
  }
}

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-decrease-entropy [-v] <updated apparent entropy> <infile>\n");
  fprintf(stderr, "Makes the found most common symbol more common (by randomly replacing other symbols with\n");
  fprintf(stderr, "the identified most common symbol), until the data has approximately the target entropy.\n");
  fprintf(stderr, "This simulates the SP 800-90B Section 4.5 Criterion (b) failure mode.\n");
  fprintf(stderr, "Outputs the resulting data to stdout in u32 format.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t *sortedData = NULL;
  size_t *nonMLSIndexes = NULL;
  size_t maxSymbolCount = 0;
  uint32_t MLS = 0;
  size_t curSymbolCount = 0;
  size_t k = 0;
  uint32_t curSymbol;
  double phat;
  double targetphat;
  struct randstate rstate;
  int opt;
  char *endptr;
  double indouble;
  double configTargetEntropy;
  size_t targetMaxSymbolCount;
  size_t nonMLScount=0;
  size_t curIndexPos;
  size_t convertToMLS;

  initGenerator(&rstate);
  seedGenerator(&rstate);

  configVerbose = 0;

  while ((opt = getopt(argc, argv, "v")) != -1) {
    switch (opt) {
      case 'v':
        // Output more debug information.
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

  endptr=NULL;
  indouble = strtod(argv[0], &endptr);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  //These double values are flags set by "strtod", so exact comparison is correct.
  if (((indouble == 0.0) && (endptr == argv[0])) || (((indouble == HUGE_VAL) || (indouble == -HUGE_VAL)) && errno == ERANGE) || (indouble < 0.0)) {
    fprintf(stderr, "Can't interpret target entropy value: %s\n", argv[0]);
    useageExit();
  }
#pragma GCC diagnostic pop
  configTargetEntropy = indouble;
  targetphat = pow(2.0, -configTargetEntropy);

  if(configVerbose > 0) fprintf(stderr, "Target p_hat = %.17g\n", targetphat);

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if ((datalen = readuint32file(infp, &data)) < 1) {
    perror("Data file is empty");
    exit(EX_DATAERR);
  }

  assert(data != NULL);

  if(configVerbose > 0) fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  assert(datalen > 0);

  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  if((sortedData = malloc(datalen*sizeof(uint32_t)))==NULL) {
    perror("Can't allocate memory for sorting");
    exit(EX_OSERR);
  }

  memcpy(sortedData, data, datalen*sizeof(uint32_t));

  //Sort the data
  if(configVerbose > 0) fprintf(stderr, "Sorting the data\n");
  qsort(sortedData, datalen, sizeof(uint32_t), uintcompare);


  if(configVerbose > 0) fprintf(stderr, "Finding the MLS\n");
  MLS = curSymbol = sortedData[0];
  curSymbolCount = 1;

  for(size_t j=1; j<datalen; j++) {
    if(sortedData[j] == curSymbol) {
       curSymbolCount ++;
    } else {
       //This is a transition.
       k++;
       if(curSymbolCount > maxSymbolCount) {
          maxSymbolCount = curSymbolCount;
          MLS = curSymbol;
       }

       curSymbolCount = 1;
       curSymbol = sortedData[j];
    }
  }
  //This is a transition.
  k++;
  if(curSymbolCount > maxSymbolCount) {
    maxSymbolCount = curSymbolCount;
    MLS = curSymbol;
  }


  free(sortedData);

  if(configVerbose > 0) {
    fprintf(stderr, "Encountered %zu distinct symbols.\n", k);
    fprintf(stderr, "Most likely symbol is 0x%08X (count %zu)\n", MLS, maxSymbolCount);
  }

  phat=((double)maxSymbolCount) / ((double)datalen);
  if(configVerbose > 0) fprintf(stderr, "p_hat = %.17g\n", phat);

  targetMaxSymbolCount = (size_t)floor(targetphat*(double)datalen);
  if(configVerbose > 0) fprintf(stderr, "Correcting to %zu MLS count\n", targetMaxSymbolCount);

  if(targetMaxSymbolCount <= maxSymbolCount) {
    fprintf(stderr, "Can't increase entropy.\n");
    exit(EX_DATAERR);
  }

  assert(datalen > maxSymbolCount);
  nonMLScount = datalen - maxSymbolCount;

  if((nonMLSIndexes = malloc(nonMLScount*sizeof(size_t)))==NULL) {
    perror("Can't allocate memory for index list");
    exit(EX_OSERR);
  }

  for(size_t index = 0; index < nonMLScount; index++) nonMLSIndexes[index] = datalen;

  /*Find all the non-MLS indexes*/
  curIndexPos = 0;
  for(size_t curIndex=0; curIndex < datalen; curIndex++) {
    if(data[curIndex] != MLS) {
      nonMLSIndexes[curIndexPos] = curIndex;
      curIndexPos++;
    }
  }
  assert(curIndexPos == nonMLScount);

  /*Determine the order that the non-MLS values will be changes to MLS values.*/
  FYShuffle64(nonMLSIndexes, nonMLScount, &rstate);

  /*Change randomly selected non-MLS values to MLS values.*/
  convertToMLS = targetMaxSymbolCount - maxSymbolCount;
  assert(convertToMLS <= nonMLScount);
  for(size_t i = 0; i < convertToMLS; i++) {
    assert(nonMLSIndexes[i] < datalen);
    assert(data[nonMLSIndexes[i]] != MLS);
    data[nonMLSIndexes[i]] = MLS;
  }

  free(nonMLSIndexes);

  if(configVerbose > 0) fprintf(stderr, "Writing the modified dataset\n");
  if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  free(data);
  
  return EX_OK;
}
