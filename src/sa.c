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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <time.h>

#include <divsufsort.h>
#include <divsufsort64.h>

#include "entlib.h"
#include "globals.h"
#include "sa.h"

/*Using the Kasai (et al.) O(n) time "13n space" algorithm.*/
//In this implementation, we use 4 byte indexes
//This accounts for the SA/LCP/rank arrays. Each of these is 4 bytes.
//There is also the base data. Thus 4n+4n+4n+n bytes.

static void sa2lcp(const statData_t *s, size_t n, saidx_t *sa, saidx_t *lcp) {
  saidx_t i, h, k, j, *rank;

  assert(n > 1);
  assert(s != NULL);
  assert(sa != NULL);
  assert(lcp != NULL);
  assert(n < SAIDX_MAX);

  if ((rank = (saidx_t *)malloc((size_t)(n + 1) * sizeof(saidx_t))) == NULL) {
    perror("Can't allocate working space for algorithm");
    exit(EX_OSERR);
  }

  for (i = 0; i <= (saidx_t)n; i++) rank[i] = -1;

  lcp[0] = -1;
  lcp[1] = 0;

  // compute rank = sa^{-1}
  for (i = 0; i <= (saidx_t)n; i++) {
    // fprintf(stderr, "S[%d]\n", i);
#ifdef SLOWCHECKS
    assert((sa[i] >= 0) && (sa[i] <= (saidx_t)n));
#endif

    rank[sa[i]] = i;
  }

  // traverse suffixes in rank order
  h = 0;

  for (i = 0; i < (saidx_t)n; i++) {
    k = rank[i];  // rank of s[i ... n-1]
    if (k > 1) {
      j = sa[k - 1];  // predecessor of s[i ... n-1]
      while ((i + h < (saidx_t)n) && (j + h < (saidx_t)n) && (s[i + h] == s[j + h])) {
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

/*Using the Kasai (et al.) O(n) time "25n space" algorithm.*/
//In this implementation, we use 8 byte indexes
//This accounts for the SA/LCP/rank arrays. Each of these is 8 bytes.
//There is also the base data. Thus 8n+8n+8n+n bytes.
static void sa2lcp64(const statData_t *s, size_t n, saidx64_t *sa, saidx64_t *lcp) {
  saidx64_t i, h, k, j, *rank;

  assert(n > 1);
  assert(s != NULL);
  assert(sa != NULL);
  assert(lcp != NULL);
  assert(n < SAIDX64_MAX);

  if ((rank = (saidx64_t *)malloc((size_t)(n + 1) * sizeof(saidx64_t))) == NULL) {
    perror("Can't allocate working space for algorithm");
    exit(EX_OSERR);
  }

  for (i = 0; i <= (saidx64_t)n; i++) rank[i] = -1;

  lcp[0] = -1;
  lcp[1] = 0;

  // compute rank = sa^{-1}
  for (i = 0; i <= (saidx64_t)n; i++) {
    // fprintf(stderr, "S[%d]\n", i);
#ifdef SLOWCHECKS
    assert((sa[i] >= 0) && (sa[i] <= (saidx64_t)n));
#endif

    rank[sa[i]] = i;
  }

  // traverse suffixes in rank order
  h = 0;

  for (i = 0; i < (saidx64_t)n; i++) {
    k = rank[i];  // rank of s[i ... n-1]
    if (k > 1) {
      j = sa[k - 1];  // predecessor of s[i ... n-1]
      while ((i + h < (saidx64_t)n) && (j + h < (saidx64_t)n) && (s[i + h] == s[j + h])) {
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

static int compareIntegerString(const statData_t *corpis, saidx_t o1, saidx_t o2, size_t n) {
  saidx_t maxoffset;
  saidx_t j;
  assert(o1 >= 0);
  assert(o2 >= 0);
  assert((size_t)o1 <= n);
  assert((size_t)o2 <= n);
  assert(corpis != NULL);
  assert(n < SAIDX_MAX);

  maxoffset = (saidx_t)n - ((o1 < o2) ? o2 : o1);

  if (o1 == o2) {
    return (0);
  }

  /*We have a virtual '$' string terminator at the end, which is lexicographically smaller than any other symbol*/
  for (j = 0; j <= maxoffset; j++) {
#ifdef SLOWCHECKS
    assert((o1 + j < (saidx_t)n) || (o2 + j < (saidx_t)n));
#endif
    if (o1 + j >= (saidx_t)n) {
      return (-1);
    } else if (o2 + j >= (saidx_t)n) {
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
static bool isValidSA(const statData_t *corpis, size_t n, saidx_t *SA) {
  size_t j;
  uint8_t *presentArray;

  if ((presentArray = malloc((n + 1) * sizeof(uint8_t))) == NULL) {
    perror("Can't allocate indicator array");
    exit(EX_OSERR);
  }

  /*First, test that this is a permutation*/
  for (j = 0; j <= n; j++) presentArray[j] = 0;

  for (j = 0; j <= n; j++) {
    if ((SA[j] < 0) || (SA[j] > (saidx_t)n)) {
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
static bool isValidSA64(const statData_t *corpis, size_t n, saidx64_t *SA) {
  size_t j;
  uint8_t *presentArray;

  if ((presentArray = malloc((n + 1) * sizeof(uint8_t))) == NULL) {
    perror("Can't allocate indicator array");
    exit(EX_OSERR);
  }

  /*First, test that this is a permutation*/
  for (j = 0; j <= n; j++) presentArray[j] = 0;

  for (j = 0; j <= n; j++) {
    if ((SA[j] < 0) || (SA[j] > (saidx64_t)n)) {
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
  return (compareIntegerString(globalS, *((const saidx_t *)o1), *((const saidx_t *)o2), globalN));
}

void calcSALCP(const statData_t *inData, size_t n, size_t k, saidx_t *SA, saidx_t *LCP) {
  size_t j;
  int32_t res;
#if STATDATA_MAX >= 256
  uint8_t *smallData;
#endif

  assert(n < SAIDX_MAX);

  // require inData[n]=$, a lexicographically minimal (virtual) symbol
  // This virtual symbol just adds a new distinct symbol.

  if ((k > 256)) {
    if (configVerbose > 3) {
      fprintf(stderr, "Calculate naive suffix array: ");
    }
    for (j = 0; j <= n; j++) {
      SA[j] = (saidx_t)j;
    }

    globalN = n;
    globalS = inData;

    qsort(SA, n + 1, sizeof(saidx_t), SAcmp);

    globalN = 0;
    globalS = NULL;
  } else {
    // The k <= 256 case
    SA[0] = (saidx_t)n;
    for (j = 1; j <= n; j++) SA[j] = -1;

    if (configVerbose > 3) {
      fprintf(stderr, "Calculate fancy suffix array with 32-bit indices: ");
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

void calcSALCP64(const statData_t *inData, size_t n, size_t k, saidx64_t *SA, saidx64_t *LCP) {
  size_t j;
  int32_t res;
  //Only supports 1 byte statData_t
  assert(STATDATA_MAX == 255);
  assert(n < SAIDX64_MAX);
  (void)k;

  // require inData[n]=$, a lexicographically minimal (virtual) symbol
  // This virtual symbol just adds a new distinct symbol.

  SA[0] = (saidx64_t)n;
  for (j = 1; j <= n; j++) SA[j] = -1;

  if (configVerbose > 3) {
    fprintf(stderr, "Calculate fancy suffix array with 64-bit indices: ");
  }

  res = divsufsort64((const sauchar_t *)inData, (saidx64_t *)(SA + 1), (saidx64_t)(n));

  assert(res == 0);

#ifdef SLOWCHECKS
  assert(isValidSA64(inData, n, SA));
#endif

  for (j = 0; j <= n; j++) LCP[j] = -1;
  sa2lcp64(inData, n, SA, LCP);

  if (configVerbose > 9) {
    for (j = 0; j <= n; j++) fprintf(stderr, "SA[%zu] = %" PRId64 "\n", j, SA[j]);
    for (j = 0; j <= n; j++) fprintf(stderr, "LCP[%zu] = %" PRId64 "\n", j, LCP[j]);
  }
}
