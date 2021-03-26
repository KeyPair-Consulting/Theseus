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
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>  // for uint32_t
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-keep-most-common [-v] inputfile\n");
  fprintf(stderr, "Keep up to the 256 most-common symbols (k symbols total), and discard the rest of the data.\n");
  fprintf(stderr, "Perform an order-preserving map on the remaining data to arrange the input symbols to (0, ..., k-1)\n");
  fprintf(stderr, "inputfile is presumed to consist of u32 samples in machine format\n");
  fprintf(stderr, "output is sent to stdout, and is u8 format\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  exit(EX_USAGE);
}

// Induce a reverse sort (greatest to smallest)
static int sizetCompare(const void *in1, const void *in2) {
  const size_t *left;
  const size_t *right;

  left = in1;
  right = in2;

  if (*left > *right) {
    return (-1);
  } else if (*left < *right) {
    return (1);
  } else {
    return (0);
  }
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data;
  size_t k;
  int opt;
  size_t *countList;
  size_t maxSymbols;

  uint32_t minVal = UINT32_MAX;
  uint32_t maxVal = 0;
  size_t countCutoff;
  size_t outputSize;

  configVerbose = 0;
  data = NULL;

  assert(PRECISION(UINT_MAX) >= 32);

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

  datalen = readuint32file(infp, &data);
  assert(data != NULL);
  assert(datalen > 0);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  // Find the largest/smallest symbols
  for (size_t j = 0; j < datalen; j++) {
    if (data[j] > maxVal) maxVal = data[j];
    if (data[j] < minVal) minVal = data[j];
  }

  assert(maxVal >= minVal);
  maxSymbols = maxVal - minVal + 1;

  if ((countList = calloc(maxSymbols, sizeof(size_t))) == NULL) {
    perror("Can't allocate memory for count list");
    exit(EX_OSERR);
  }

  // How many times does each symbol occur?
  for (size_t j = 0; j < datalen; j++) {
    countList[data[j] - minVal]++;
  }

  // Establish the smallest count that gets an output symbol
  if (maxSymbols <= UINT8_MAX + 1) {
    countCutoff = 0;
  } else {
    size_t *sortedCountList;
    if ((sortedCountList = calloc(maxSymbols, sizeof(size_t))) == NULL) {
      perror("Can't allocate memory for sorted count list");
      exit(EX_OSERR);
    }

    // Make a copy of the count list, so we can find the count cutoff
    memcpy(sortedCountList, countList, sizeof(size_t) * maxSymbols);

    // Sort the count list from greatest to smallest
    qsort(sortedCountList, maxSymbols, sizeof(size_t), sizetCompare);
    assert(sortedCountList[0] >= sortedCountList[maxSymbols - 1]);

    // Make an initial estimate for the countCutoff
    countCutoff = sortedCountList[UINT8_MAX];

    // This may be too low if there are other symbols with this count; look at the next most common symbol to check
    if (countCutoff == sortedCountList[UINT8_MAX + 1]) {
      // This cutoff would lead to too many symbols. Increase the cutoff.
      // Note: this results in < 256 symbols.
      countCutoff++;
    }
    free(sortedCountList);
  }

  if (configVerbose > 0) fprintf(stderr, "Symbol Count Cutoff: %zu\n", countCutoff);

  // Count the actual number of output symbols and change the countList to a translation array
  k = 0;
  for (size_t j = 0; j < maxSymbols; j++) {
    if (countList[j] >= countCutoff) {
      // There are a suitable number of these symbols. Its translated value is k.
      countList[j] = k;
      k++;
    } else {
      // flag this symbol as discarded
      countList[j] = SIZE_MAX;
    }
  }

  if (configVerbose > 0) fprintf(stderr, "Total output alphabet size: %zu\n", k);
  assert(k > 0);
  assert(k <= SIZE_MAX);
  assert(k <= UINT8_MAX + 1);

  // Keep track of the output length
  outputSize = 0;

  // Output the translated data
  for (size_t j = 0; j < datalen; j++) {
    size_t curSymbolCount = countList[data[j] - minVal];
    if (curSymbolCount < k) {
      uint8_t out = (uint8_t)curSymbolCount;
      if (fwrite(&out, sizeof(uint8_t), 1, stdout) != 1) {
        perror("Can't write to output");
        exit(EX_OSERR);
      }
      outputSize++;
    }
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Total symbols output: %zu (retained %g%% of input data)\n", outputSize, 100.0 * ((double)outputSize) / ((double)datalen));
  }

  free(data);
  free(countList);
  return EX_OK;
}
