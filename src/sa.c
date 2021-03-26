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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>

#include <divsufsort.h>

#include "entlib.h"
#include "globals.h"
#include "sa.h"

/*Using the Kasai (et al.) O(n) time "13n space" algorithm.*/
/*In this implementation, we use 4 byte indexes*/
/*Note that indexes should be signed, so the next natural size is int64_t*/

static void sa2lcp(const statData_t *s, size_t n, SAINDEX *sa, SAINDEX *lcp) {
  SAINDEX i, h, k, j, *rank;

  assert(n > 1);
  assert(s != NULL);
  assert(sa != NULL);
  assert(lcp != NULL);
  assert(n <= SAINDEX_MAX);

  if ((rank = (SAINDEX *)malloc((size_t)(n + 1) * sizeof(SAINDEX))) == NULL) {
    perror("Can't allocate working space for algorithm");
    exit(EX_OSERR);
  }

  for (i = 0; i <= (SAINDEX)n; i++) rank[i] = -1;

  lcp[0] = -1;
  lcp[1] = 0;

  assert(n < SAINDEX_MAX);

  // compute rank = sa^{-1}
  for (i = 0; i <= (SAINDEX)n; i++) {
    // fprintf(stderr, "S[%d]\n", i);
#ifdef SLOWCHECKS
    assert((sa[i] >= 0) && (sa[i] <= (SAINDEX)n));
#endif

    rank[sa[i]] = i;
  }

  // traverse suffixes in rank order
  h = 0;

  for (i = 0; i < (SAINDEX)n; i++) {
    k = rank[i];  // rank of s[i ... n-1]
    if (k > 1) {
      j = sa[k - 1];  // predecessor of s[i ... n-1]
      while ((i + h < (SAINDEX)n) && (j + h < (SAINDEX)n) && (s[i + h] == s[j + h])) {
        h++;
      }
      lcp[k] = h;
    }
    if (h > 0) {
      h--;
    }
  }

  free(rank);
}

static int compareIntegerString(const statData_t *corpis, SAINDEX o1, SAINDEX o2, size_t n) {
  SAINDEX maxoffset;
  SAINDEX j;
  assert(o1 >= 0);
  assert(o2 >= 0);
  assert((size_t)o1 <= n);
  assert((size_t)o2 <= n);
  assert(corpis != NULL);
  assert(n <= SAINDEX_MAX);

  maxoffset = (SAINDEX)n - ((o1 < o2) ? o2 : o1);

  if (o1 == o2) {
    return (0);
  }

  /*We have a virtual '$' string terminator at the end, which is lexigraphically smaller than any other symbol*/
  for (j = 0; j <= maxoffset; j++) {
#ifdef SLOWCHECKS
    assert((o1 + j < (SAINDEX)n) || (o2 + j < (SAINDEX)n));
#endif
    if (o1 + j >= (SAINDEX)n) {
      return (-1);
    } else if (o2 + j >= (SAINDEX)n) {
      return (1);
    } else if (corpis[o1 + j] < corpis[o2 + j]) {
      return (-1);
    } else if (corpis[o1 + j] > corpis[o2 + j]) {
      return (1);
    }
  }

  // We shouldn't have arrived here
  assert(false == true);
}

#ifdef SLOWCHECKS
static bool isValidSA(const statData_t *corpis, size_t n, SAINDEX *SA) {
  size_t j;
  uint8_t *presentArray;

  if ((presentArray = malloc((n + 1) * sizeof(uint8_t))) == NULL) {
    perror("Can't allocate indicator array");
    exit(EX_OSERR);
  }

  /*First, test that this is a permutation*/
  for (j = 0; j <= n; j++) presentArray[j] = 0;

  for (j = 0; j <= n; j++) {
    if ((SA[j] < 0) || (SA[j] > (SAINDEX)n)) {
      free(presentArray);
      return (false);
    }
    if (presentArray[SA[j]] == 1) {
      free(presentArray);
      return (false);
    }
    presentArray[SA[j]] = 1;
  }

  for (j = 0; j <= n; j++) {
    if (presentArray[j] != 1) {
      free(presentArray);
      return (false);
    }
  }

  free(presentArray);

  /*Now check the ordering*/
  for (j = 1; j <= n; j++) {
    if (compareIntegerString(corpis, SA[j - 1], SA[j], n) != -1) return (false);
  }

  return true;
}
#endif

static const statData_t *globalS;
static size_t globalN;

static int SAcmp(const void *o1, const void *o2) {
  return (compareIntegerString(globalS, *((const SAINDEX *)o1), *((const SAINDEX *)o2), globalN));
}

void calcSALCP(const statData_t *inData, size_t n, size_t k, SAINDEX *SA, SAINDEX *LCP) {
  size_t j;
  int32_t res;
#if STATDATA_MAX >= 256
  uint8_t *smallData;
#endif

  assert(n < SAINDEX_MAX);

  // require inData[n]=$, a lexigraphically minimal (virtual) symbol
  // This virtual symbol just adds a new distinct symbol.

  if ((k > 256)) {
    if (configVerbose > 3) {
      fprintf(stderr, "Calculate naive suffix array: ");
    }
    for (j = 0; j <= n; j++) {
      SA[j] = (SAINDEX)j;
    }

    globalN = n;
    globalS = inData;

    qsort(SA, n + 1, sizeof(SAINDEX), SAcmp);

    globalN = 0;
    globalS = NULL;
  } else {
    // The k <= 256 case
    SA[0] = (SAINDEX)n;
    for (j = 1; j <= n; j++) SA[j] = -1;

    if (configVerbose > 3) {
      fprintf(stderr, "Calculate fancy suffix array: ");
    }

#if STATDATA_MAX >= 256
    if ((smallData = (uint8_t *)malloc((n) * sizeof(uint8_t))) == NULL) {
      perror("Can't allocate smaller array");
      exit(EX_OSERR);
    }

    for (j = 0; j < n; j++) {
      assert(inData[j] < 256);
      smallData[j] = (uint8_t)(inData[j]);
    }

    res = divsufsort((sauchar_t *)smallData, (saidx_t *)(SA + 1), (saidx_t)(n));
    free(smallData);
#else
    res = divsufsort((const sauchar_t *)inData, (saidx_t *)(SA + 1), (saidx_t)(n));
#endif

    assert(res == 0);
  }

#ifdef SLOWCHECKS
  assert(isValidSA(inData, n, SA));
#endif

  for (j = 0; j <= n; j++) LCP[j] = -1;
  sa2lcp(inData, n, SA, LCP);

  if (configVerbose > 9) {
    for (j = 0; j <= n; j++) fprintf(stderr, "SA[%zu] = %d\n", j, SA[j]);
    for (j = 0; j <= n; j++) fprintf(stderr, "LCP[%zu] = %d\n", j, LCP[j]);
  }
}
