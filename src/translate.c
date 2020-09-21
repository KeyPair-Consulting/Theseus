/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include "translate.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include "globals.h"

static int statDataCompare(const void *in1, const void *in2) {
  const statData_t *left;
  const statData_t *right;

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

static size_t sortRelabel(statData_t *data, size_t datalen, size_t *k, double *translatedMedian) {
  statData_t *sorteddata = NULL;
  statData_t *rewritetable = NULL;
  size_t i;
  bool needToRewrite = false;
  statData_t *cursymbol;
  statData_t medianLow, medianHigh;

  assert(data != NULL);
  assert(k != NULL);
  assert(translatedMedian != NULL);
  assert(*k >= 2);
  assert(datalen > 2);

  /* 2 L + 1.39*L*log2(L) + L*log2(l) */
  if (configVerbose > 0) {
    fprintf(stderr, "Checking if we need to translate data: ");
  }

  if ((sorteddata = malloc(datalen * sizeof(statData_t))) == NULL) {
    perror("Can't allocate array for sorted data");
    exit(EX_OSERR);
  }

  /* L ops */
  memcpy(sorteddata, data, datalen * sizeof(statData_t));

  /* 1.39*L*log2(L) ops on average */
  qsort(sorteddata, datalen, sizeof(statData_t), statDataCompare);

  if ((rewritetable = malloc(datalen * sizeof(statData_t))) == NULL) {
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
      if (!needToRewrite && (rewritetable[*k] != (statData_t)(*k))) {
        needToRewrite = true;
      }
    }
  }
  (*k)++;

  medianLow = sorteddata[datalen / 2 - ((datalen % 2 == 0) ? 1 : 0)];
  medianHigh = sorteddata[datalen / 2];

  assert(sorteddata != NULL);
  free(sorteddata);
  sorteddata = NULL;

  /* L * log2(l) */
  if (needToRewrite) {
    /* L ops */
    if (configVerbose > 0) {
      fprintf(stderr, "Translating data\n");
    }
    for (i = 0; i < datalen; i++) {
      /* each on takes log2( l ) */
      if ((cursymbol = bsearch(data + i, rewritetable, *k, sizeof(statData_t), statDataCompare)) == NULL) {
        perror("Can't find the correct value for translation.");
        assert(rewritetable != NULL);
        free(rewritetable);
        rewritetable = NULL;
        exit(EX_DATAERR);
      }

      assert(cursymbol >= rewritetable);
      data[i] = (statData_t)(cursymbol - rewritetable);
    }
    if (configVerbose > 0) {
      fprintf(stderr, "Found %zu symbols total.\n", *k);
    }

    // Translate the median values
    if ((cursymbol = bsearch(&medianLow, rewritetable, *k, sizeof(statData_t), statDataCompare)) == NULL) {
      perror("Can't find the correct value for translation.");
      assert(rewritetable != NULL);
      free(rewritetable);
      rewritetable = NULL;
      exit(EX_DATAERR);
    }

    assert(cursymbol >= rewritetable);
    medianLow = (statData_t)(cursymbol - rewritetable);

    if ((cursymbol = bsearch(&medianHigh, rewritetable, *k, sizeof(statData_t), statDataCompare)) == NULL) {
      perror("Can't find the correct value for translation.");
      assert(rewritetable != NULL);
      free(rewritetable);
      rewritetable = NULL;
      exit(EX_DATAERR);
    }

    assert(cursymbol >= rewritetable);
    medianHigh = (statData_t)(cursymbol - rewritetable);
  } else {
    if (configVerbose > 0) {
      fprintf(stderr, "No\n");
    }
  }

  assert(rewritetable != NULL);
  free(rewritetable);
  rewritetable = NULL;

  // Note, neither overflow nor underflow are an issue here.
  *translatedMedian = ((double)medianHigh + (double)medianLow) / 2.0;
  return (needToRewrite);
}

static bool tableRelabel(statData_t *S, size_t L, size_t *k, double *translatedMedian) {
  int64_t *rewritetable;
  size_t *symbolCount;
  size_t i, j;
  bool translateNeeded;
  size_t count;
  size_t targetCount, medianSlop;
  size_t newcount;

  assert(S != NULL);
  assert(k != NULL);
  assert(translatedMedian != NULL);
  assert(*k >= 2);
  assert(*k - 1 <= STATDATA_MAX);
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
  medianSlop = L % 2;
  if (configVerbose > 2) fprintf(stderr, "targetCount: %zu, medianSlop: %zu\n", targetCount, medianSlop);

  for (i = 0; i < (*k); i++) {
    if (symbolCount[i] != 0) {
      // record the symbol map
      assert(rewritetable[i] == -1);
      rewritetable[i] = (int64_t)j;
      newcount = count + symbolCount[i];

      if (count < targetCount) {
        if (targetCount == newcount) {
          if (configVerbose > 2) fprintf(stderr, "Prior count under target, newcount equal to target. ");
          if (medianSlop == 1) {
            // Odd case
            *translatedMedian = (double)j + 1.0;
            if (configVerbose > 2) fprintf(stderr, "Select next symbol. ");
          } else {
            // even case
            *translatedMedian = (double)j + 0.5;
            if (configVerbose > 2) fprintf(stderr, "Select space halfway to next symbol. ");
          }
        } else if (targetCount < newcount) {
          // For both the even and odd case, there is no averaging necessary in this case
          *translatedMedian = (double)j;
          if (configVerbose > 2) fprintf(stderr, "Prior count under target, newcount is above target. Median %.17g.\n", *translatedMedian);
        }
      }

      count = newcount;
      j++;
    }
  }

  if (*k != j) {
    *k = j;

    translateNeeded = true;
    if (configVerbose > 0) {
      fprintf(stderr, "Translation is necessary... Found %zu symbols total.\n", j);
    }
    /* L ops */
    for (i = 0; i < L; i++) {
      assert(rewritetable[S[i]] >= 0);
      assert(rewritetable[S[i]] < (int64_t)(*k));
      S[i] = (statData_t)rewritetable[S[i]];
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

bool translate(statData_t *S, size_t L, size_t *k, double *translatedMedian) {
  size_t i;
  size_t maxSymbols;
  bool didTranslate;

  assert(S != NULL);
  assert(translatedMedian != NULL);
  assert(L >= 1);

  if (L <= 2) {
    didTranslate = false;
    if (L == 1) {
      if (S[0] != 0) {
        S[0] = 0;
        didTranslate = true;
      }
      *translatedMedian = 0;
      *k = 1;
    } else {
      // L == 2
      if (S[0] == S[1]) {
        *k = 1;
        *translatedMedian = 0.0;
        didTranslate = (S[0] != 0);
        S[0] = S[1] = 0;
      } else {
        didTranslate = !((S[0] == 0 && S[1] == 1) || (S[0] == 1 && S[1] == 0));
        *translatedMedian = 0.5;
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
    didTranslate = tableRelabel(S, L, k, translatedMedian);
  } else {
    if (configVerbose > 0) {
      fprintf(stderr, "Sorting based translation approach.\n");
    }
    /*It is likely quicker to just sort the list and relabel*/
    didTranslate = sortRelabel(S, L, k, translatedMedian);
  }

  return didTranslate;
}
