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
  fprintf(stderr, "u32-translate-data [-v] inputfile\n");
  fprintf(stderr, "Perform an order-preserving map to arrange the input symbols to (0, ..., k-1)\n");
  fprintf(stderr, "inputfile is presumed to consist of u32 in machine format\n");
  fprintf(stderr, "output is sent to stdout, and is u32 in machine format\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-n\tNo data output. Report number of symbols on stdout.\n");
  exit(EX_USAGE);
}

static int u32Compare(const void *in1, const void *in2) {
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

static size_t u32sortRelabel(uint32_t *data, size_t datalen, size_t *k) {
  uint32_t *sorteddata = NULL;
  uint32_t *rewritetable = NULL;
  size_t i;
  bool needToRewrite = false;
  uint32_t *cursymbol;

  assert(data != NULL);
  assert(k != NULL);
  assert(*k >= 2);
  assert(datalen > 2);

  /* 2 L + 1.39*L*log2(L) + L*log2(l) */
  if (configVerbose > 0) {
    fprintf(stderr, "Checking if we need to translate data: ");
  }

  if ((sorteddata = malloc(datalen * sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for sorted data");
    exit(EX_OSERR);
  }

  /* L ops */
  memcpy(sorteddata, data, datalen * sizeof(uint32_t));

  /* 1.39*L*log2(L) ops on average */
  qsort(sorteddata, datalen, sizeof(uint32_t), u32Compare);

  if ((rewritetable = malloc(datalen * sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for rewrite table");
    assert(sorteddata != NULL);
    free(sorteddata);
    sorteddata = NULL;
    exit(EX_OSERR);
  }

  /* L ops */
  rewritetable[0] = sorteddata[0];
  *k = 0;
  needToRewrite = false;
  if (rewritetable[*k] != *k) {
    needToRewrite = true;
  }

  for (i = 1; i < datalen; i++) {
    if (sorteddata[i] != rewritetable[*k]) {
      (*k)++;
      rewritetable[*k] = sorteddata[i];
      if (!needToRewrite && (rewritetable[*k] != (uint32_t)(*k))) {
        needToRewrite = true;
      }
    }
  }
  (*k)++;

  assert(sorteddata != NULL);
  free(sorteddata);
  sorteddata = NULL;

  /* L * log2(l) */
  if (needToRewrite) {
    uint32_t maxSymbol = 0;
    /* L ops */
    if (configVerbose > 0) {
      fprintf(stderr, "Translating data\n");
    }
    for (i = 0; i < datalen; i++) {
      /* each on takes log2( l ) */
      if ((cursymbol = bsearch(data + i, rewritetable, *k, sizeof(uint32_t), u32Compare)) == NULL) {
        perror("Can't find the correct value for translation.");
        assert(rewritetable != NULL);
        free(rewritetable);
        rewritetable = NULL;
        exit(EX_DATAERR);
      }

      assert(cursymbol >= rewritetable);
      data[i] = (uint32_t)(cursymbol - rewritetable);
      if (data[i] > maxSymbol) maxSymbol = data[i];
    }
    if (configVerbose > 0) {
      fprintf(stderr, "Found %zu symbols total (max symbol %u).\n", *k, maxSymbol);
    }
  } else {
    if (configVerbose > 0) {
      fprintf(stderr, "No\n");
    }
  }

  assert(rewritetable != NULL);
  free(rewritetable);
  rewritetable = NULL;

  // Note, neither overflow nor underflow are an issue here.
  return (needToRewrite);
}

static bool u32tableRelabel(uint32_t *S, size_t L, size_t *k) {
  int64_t *rewritetable;
  size_t *symbolCount;
  size_t i, j;
  bool translateNeeded;
  size_t count;
  size_t targetCount;
  size_t newcount;

  assert(S != NULL);
  assert(k != NULL);
  assert(*k >= 2);
  assert(L > 2);

  /* 2 L + 3 k ops total */
  if ((rewritetable = malloc(sizeof(int64_t) * (*k))) == NULL) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }

  if ((symbolCount = malloc(sizeof(size_t) * (*k))) == NULL) {
    perror("Memory allocation error");
    assert(rewritetable != NULL);
    free(rewritetable);
    rewritetable = NULL;
    exit(EX_OSERR);
  }

  /* 2 k ops */
  for (j = 0; j < (*k); j++) {
    symbolCount[j] = 0;
  }

  for (j = 0; j < (*k); j++) {
    rewritetable[j] = -1;
  }

  /* L ops */
  for (j = 0; j < L; j++) {
    assert(S[j] < (*k));
    symbolCount[S[j]]++;
  }

  /* k ops */
  j = 0;
  count = 0;
  targetCount = L / 2;
  if (configVerbose > 2) fprintf(stderr, "targetCount: %zu\n", targetCount);

  for (i = 0; i < (*k); i++) {
    if (symbolCount[i] != 0) {
      // record the symbol map
      assert(rewritetable[i] == -1);
      rewritetable[i] = (int64_t)j;
      newcount = count + symbolCount[i];

      count = newcount;
      j++;
    }
  }

  if (*k != j) {
    uint32_t maxSymbol = 0;
    *k = j;

    translateNeeded = true;
    if (configVerbose > 0) {
      fprintf(stderr, "Translation is necessary... Found %zu symbols total", j);
    }
    /* L ops */
    for (i = 0; i < L; i++) {
      assert(rewritetable[S[i]] >= 0);
      assert(rewritetable[S[i]] < (int64_t)(*k));
      S[i] = (uint32_t)rewritetable[S[i]];
      if (S[i] > maxSymbol) maxSymbol = S[i];
    }

    if (configVerbose > 0) {
      fprintf(stderr, " (max Symbol = %u)\n", maxSymbol);
    }
  } else {
    translateNeeded = false;
    if (configVerbose > 0) {
      fprintf(stderr, "No translation is necessary.\n");
    }
  }

  assert(rewritetable != NULL);
  free(rewritetable);
  rewritetable = NULL;

  assert(symbolCount != NULL);
  free(symbolCount);
  symbolCount = NULL;

  return translateNeeded;
}

static bool u32translate(uint32_t *S, size_t L, size_t *k) {
  size_t i;
  size_t maxSymbols;
  bool didTranslate;

  assert(S != NULL);
  assert(L >= 1);

  if (L <= 2) {
    didTranslate = false;
    if (L == 1) {
      if (S[0] != 0) {
        S[0] = 0;
        didTranslate = true;
      }
      *k = 1;
    } else {
      // L == 2
      if (S[0] == S[1]) {
        *k = 1;
        didTranslate = (S[0] != 0);
        S[0] = S[1] = 0;
      } else {
        didTranslate = !((S[0] == 0 && S[1] == 1) || (S[0] == 1 && S[1] == 0));
        *k = 2;
        if (S[0] < S[1]) {
          S[0] = 0;
          S[1] = 1;
        } else {
          S[0] = 1;
          S[1] = 0;
        }
      }
    }
    return (didTranslate);
  }

  *k = 0;
  for (i = 0; i < L; i++) {
    if (*k < S[i]) {
      *k = S[i];
    }
  }

  (*k)++;

  maxSymbols = ((*k < L) ? *k : L);

  if (configVerbose > 0) {
    fprintf(stderr, "At most %zu symbols.\n", *k);
  }

  // Catch the all zeros case
  if (*k < 2) return false;

  /* 2 L + 1.39*L*log2(L) + L*log2(l) */
  if (((2.0 * (double)(L + *k)) < ((double)L) * (1.39 * log2((double)L) + log2((double)maxSymbols))) && (log2((double)*k) < 28.0)) {
    if (configVerbose > 0) {
      fprintf(stderr, "Table based translation approach.\n");
    }
    didTranslate = u32tableRelabel(S, L, k);
  } else {
    if (configVerbose > 0) {
      fprintf(stderr, "Sorting based translation approach.\n");
    }
    /*It is likely quicker to just sort the list and relabel*/
    didTranslate = u32sortRelabel(S, L, k);
  }

  return didTranslate;
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data;
  size_t k;
  int opt;
  bool configSymbolsOnly;

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

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  u32translate(data, datalen, &k);
  if (configVerbose > 0) {
    fprintf(stderr, "Found %zu symbols\n", k);
  }

  if (configSymbolsOnly) {
    printf("%zu\n", k);
  } else {
    if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
      perror("Can't write to output");
      exit(EX_OSERR);
    }
  }

  free(data);
  return EX_OK;
}
