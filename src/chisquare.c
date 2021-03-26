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
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "cephes.h"
#include "fancymath.h"
#include "precision.h"
#include "randlib.h"
#include "translate.h"

#include "globals-inst.h"

/*
 * In this implementation, we calculate the pValue for results from a Pearson's Chi-Square test.
 * In the Chi-squared test, the CDF of the Chi-squared distribution is based on the lower
 * regularized gamma function.
 * See https://en.wikipedia.org/wiki/Chi-squared_distribution#Cumulative_distribution_function.
 * Thus, the p-value is just 1-P(k/2,x/2)=Q(k/2,x/2),
 * where P and Q denote the lower and upper regularized gamma functions (respectively),
 * k is the degrees of freedom, and x is the calculated chi-squared test statistic.
 */

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "chisquare [-v] [-l <index>,<samples> ] <inputfile>\nor\n");
  fprintf(stderr, "chisquare [-v] [-k <m>] [-s <m>] [-r]\n");
  fprintf(stderr, "inputfile is assumed to be a sequence of " STATDATA_STRING " integers\n");
  fprintf(stderr, "-v \t verbose mode.\n");
  fprintf(stderr, "-l <index>,<samples>\tRead the <index> substring of length <samples>.\n");
  fprintf(stderr, "-r \t instead of doing testing on provided data use a random iid variable.\n");
  fprintf(stderr, "-k <m> \t Use an alphabet of <m> values. (default k=2)\n");
  fprintf(stderr, "-s <m> \t Use a sample set of <m> values. (default m=1000000)\n");
  exit(EX_USAGE);
}

static bool binaryChiSquareIndependence(const statData_t *data, size_t datalen, size_t oneCount) {
  uint32_t m;
  size_t zeroCount;
  size_t leastCommonSymbolCount;
  size_t counts[2048];
  uint32_t curOneCount;
  uint32_t curSymbol;
  size_t blockCount;
  uint32_t bitMask;
  double e, T, pValue;
  double p0, p1;
  uint32_t j;
  uint32_t k;
  double phat;

  assert(datalen >= 5);
  zeroCount = datalen - oneCount;
  leastCommonSymbolCount = (zeroCount <= oneCount) ? zeroCount : oneCount;
  phat = ((double)leastCommonSymbolCount) / ((double)datalen);

  for (m = 11; m >= 2; m--) {
    if (integerPow(phat, m) * floor((double)datalen / (double)m) >= 5.0) break;
  }

  if (m < 2) {
    if (configVerbose > 0) {
      fprintf(stderr, "Insufficient variation for 2-tuples. Test Fails.\n");
    }
    return false;
  }

  assert(m <= datalen);

  if (configVerbose > 0) {
    fprintf(stderr, "Binary Chi Squared Independence: m = %u\n", m);
  }

  blockCount = datalen / m;
  bitMask = (1U << m) - 1U;

  if (configVerbose > 0) {
    fprintf(stderr, "Bitmask is 0x%08X\n", bitMask);
  }

  for (j = 0; j <= bitMask; j++) {
    counts[j] = 0;
  }

  for (k = 0; k < blockCount; k++) {
    curSymbol = 0;
    for (j = 0; j < m; j++) {
      curSymbol = ((curSymbol << 1) | (data[k * m + j] & 0x01));
    }

    counts[curSymbol]++;
  }

  /*We've counted, now calculate the test statistic*/
  p0 = (double)zeroCount / (double)datalen;
  p1 = (double)oneCount / (double)datalen;
  T = 0.0;
  for (j = 0; j <= bitMask; j++) {
    curOneCount = (unsigned)__builtin_popcount(j);
    e = integerPow(p0, m - curOneCount) * integerPow(p1, curOneCount) * (double)blockCount;
    T += square((double)counts[j] - e) / e;
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Binary Chi-Square Independence: Test Statistic = %.17g\n", T);
    fprintf(stderr, "Binary Chi-Square Independence: df = %u\n", bitMask - 1);
  }
  pValue = cephes_igamc(((double)bitMask - 1.0) / 2.0, T / 2.0);
  if (configVerbose > 0) {
    fprintf(stderr, "Binary Chi-Square Independence: p-value = %.17g\n", pValue);
  }
  return (pValue >= 0.001);
}

static bool binaryChiSquareFit(const statData_t *data, size_t datalen, size_t oneCount) {
  double p;
  size_t perBucket;
  size_t d, j;
  double e0, e1, T, pValue;
  const statData_t *curPos;
  size_t curSum;

  p = (double)oneCount / (double)datalen;
  perBucket = datalen / 10;
  e1 = (double)perBucket * p;
  e0 = (double)perBucket - e1;

  assert(p * (double)perBucket >= 5.0);

  curPos = data;
  T = 0.0;
  for (d = 0; d < 10; d++) {
    curSum = 0;
    for (j = 0; j < perBucket; j++) {
      curSum += *curPos & 0x01;
      curPos++;
    }
    T += square((double)curSum - e1) / e1;
    T += square((double)(perBucket - curSum) - e0) / e0;
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Binary Chi-Square Goodness-of-Fit: Test Statistic = %.17g\n", T);
  }
  pValue = cephes_igamc(4.5, T / 2.0);
  if (configVerbose > 0) {
    fprintf(stderr, "Binary Chi-Square Goodness-of-Fit: p-value = %.17g\n", pValue);
  }
  return (pValue >= 0.001);
}

struct chiSquareData {
  uint64_t index;
  double expected;
  size_t bin;
};

static int chiSquareIndexSort(const void *in1, const void *in2) {
  const struct chiSquareData *left;
  const struct chiSquareData *right;

  left = in1;
  right = in2;

  if (left->index < right->index) {
    return (-1);
  } else if (left->index > right->index) {
    return (1);
  } else {
    return (0);
  }
}

static int chiSquareExpectedSort(const void *in1, const void *in2) {
  const struct chiSquareData *left;
  const struct chiSquareData *right;

  left = in1;
  right = in2;

  if (left->expected < right->expected) {
    return (-1);
  } else if (left->expected > right->expected) {
    return (1);
  } else {
    if (left->index < right->index) {
      return (-1);
    } else if (left->index > right->index) {
      return (1);
    } else {
      return (0);
    }
  }
}

/*Takes structures that have populated index and expected, and assigns the bin*/
static size_t chiSquareBin(struct chiSquareData *chiSquareTable, double *binExpectations, size_t k) {
  size_t curBin = 0;
  int64_t j;

  assert(chiSquareTable != NULL);
  assert(binExpectations != NULL);
  assert(k > 2);
  assert(k <= INT64_MAX);

  for (j = 0; j < (int64_t)k; j++) {
    binExpectations[j] = -1.0;
  }

  // First, sort these by the expected value from least to most.
  qsort(chiSquareTable, k, sizeof(struct chiSquareData), chiSquareExpectedSort);

  binExpectations[0] = 0.0;

  for (j = 0; j < (int64_t)k; j++) {
    chiSquareTable[j].bin = curBin;
    binExpectations[curBin] += chiSquareTable[j].expected;
    if (configVerbose > 3) {
      fprintf(stderr, "Adding index %zu (expectation %.17g) to bin %zu (new expectation %.17g)\n", j, chiSquareTable[j].expected, curBin, binExpectations[curBin]);
    }

    if (binExpectations[curBin] >= 5.0) {
      curBin++;
      binExpectations[curBin] = 0.0;
      if (configVerbose > 3) {
        fprintf(stderr, "New bin created\n");
      }
    }
  }

  assert(curBin > 0);

  if ((binExpectations[curBin] >= 0.0) && (binExpectations[curBin] < 5.0)) {
    // need to undo the last bin
    binExpectations[curBin - 1] += binExpectations[curBin];
    binExpectations[curBin] = -1.0;

    for (j = (int64_t)k - 1; (j >= 0) && (chiSquareTable[j].bin == curBin); j--) {
      chiSquareTable[j].bin = curBin - 1;
    }

    curBin--;
  }

  qsort(chiSquareTable, k, sizeof(struct chiSquareData), chiSquareIndexSort);

  return (curBin + 1);
}

static bool nonbinaryChiSquareIndependence(const statData_t *data, size_t datalen, size_t *symbolCounts, size_t k) {
  size_t nbin;
  size_t i, j;
  struct chiSquareData *chiSquareLookup;
  double *binExpectations;
  size_t *binCounts;
  size_t curIndex;
  size_t curBin;
  double T;
  double pValue;

  if ((chiSquareLookup = malloc(k * k * sizeof(struct chiSquareData))) == NULL) {
    perror("Can't allocate chiSquareLookup structure");
    exit(EX_OSERR);
  }

  if ((binExpectations = malloc(k * k * sizeof(double))) == NULL) {
    perror("Can't allocate binExpectations");
    exit(EX_OSERR);
  }

  for (i = 0; i < k; i++) {
    for (j = 0; j < k; j++) {
      curIndex = i * k + j;
      chiSquareLookup[curIndex].index = curIndex;
      chiSquareLookup[curIndex].expected = floor((double)datalen / 2.0) * ((double)symbolCounts[i]) * ((double)symbolCounts[j]) / (((double)datalen) * ((double)datalen));
    }
  }

  nbin = chiSquareBin(chiSquareLookup, binExpectations, k * k);

  assert(nbin > k);
  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Independence: nbin = %zu\n", nbin);
  }

  // Chi square test step 1
  if ((binCounts = malloc(nbin * sizeof(size_t))) == NULL) {
    perror("Can't allocate binCounts");
    exit(EX_OSERR);
  }

  for (j = 0; j < nbin; j++) binCounts[j] = 0;

  for (j = 0; j < datalen - 1; j += 2) {
    curIndex = k * data[j] + data[j + 1];
    assert(curIndex < k * k);
    curBin = chiSquareLookup[curIndex].bin;
    assert(curBin < nbin);

    binCounts[curBin]++;
  }

  // Chi Square test step 2
  T = 0.0;
  for (j = 0; j < nbin; j++) {
    if (configVerbose > 2) {
      fprintf(stderr, "bin index %zu: binCount = %zu, binExp = %.17g\n", j, binCounts[j], binExpectations[j]);
    }
    T += square(((double)binCounts[j]) - binExpectations[j]) / binExpectations[j];
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Independence: Test Statistic = %.17g\n", T);
    fprintf(stderr, "Non-Binary Chi-Square Independence: df = %zu\n", nbin - k);
  }

  pValue = cephes_igamc(((double)(nbin - k)) / 2.0, T / 2.0);

  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Independence: p-value = %.17g\n", pValue);
  }

  assert(chiSquareLookup != NULL);
  free(chiSquareLookup);
  chiSquareLookup = NULL;

  assert(binExpectations != NULL);
  free(binExpectations);
  binExpectations = NULL;

  assert(binCounts != NULL);
  free(binCounts);
  binCounts = NULL;

  return (pValue >= 0.001);
}

static bool nonbinaryChiSquareFit(const statData_t *data, size_t datalen, size_t *symbolCounts, size_t k) {
  size_t nbin;
  size_t i, j;
  struct chiSquareData *chiSquareLookup;
  double *binExpectations;
  size_t *binCounts;
  size_t curBin;
  double T;
  double pValue;
  size_t subsequenceSize;

  if ((chiSquareLookup = malloc(k * sizeof(struct chiSquareData))) == NULL) {
    perror("Can't allocate chiSquareLookup structure");
    exit(EX_OSERR);
  }

  if ((binExpectations = malloc(k * sizeof(double))) == NULL) {
    perror("Can't allocate binExpectations");
    exit(EX_OSERR);
  }

  for (i = 0; i < k; i++) {
    chiSquareLookup[i].index = i;
    chiSquareLookup[i].expected = ((double)symbolCounts[i] / (double)datalen) * floor(((double)datalen) / 10.0);
  }

  nbin = chiSquareBin(chiSquareLookup, binExpectations, k);

  assert(nbin > 1);

  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Goodness-of-Fit: nbin = %zu\n", nbin);
  }

  // Chi square test step 1
  if ((binCounts = malloc(nbin * sizeof(size_t))) == NULL) {
    perror("Can't allocate binCounts");
    exit(EX_OSERR);
  }

  subsequenceSize = datalen / 10;

  T = 0.0;
  for (i = 0; i < 10; i++) {
    // Clear out the bincounts
    for (j = 0; j < nbin; j++) binCounts[j] = 0;

    // Account for the current dataset
    for (j = 0; j < subsequenceSize; j++) {
      curBin = chiSquareLookup[data[i * subsequenceSize + j]].bin;
      assert(curBin < nbin);
      binCounts[curBin]++;
    }

    // Chi Square test step 2
    for (j = 0; j < nbin; j++) {
      if (configVerbose > 2) fprintf(stderr, "BinCount[%zu] = %zu, expected %.17g\n", j, binCounts[j], binExpectations[j]);
      T += square(((double)binCounts[j]) - binExpectations[j]) / binExpectations[j];
    }
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Goodness-of-Fit: Test Statistic = %.17g\n", T);
    fprintf(stderr, "Non-Binary Chi-Square Goodness-of-Fit: df = %zu\n", 9 * (nbin - 1));
  }

  pValue = cephes_igamc(((double)(9 * (nbin - 1))) / 2.0, T / 2.0);

  if (configVerbose > 0) {
    fprintf(stderr, "Non-Binary Chi-Square Goodness-of-Fit: p-value = %.17g\n", pValue);
  }

  assert(chiSquareLookup != NULL);
  free(chiSquareLookup);
  chiSquareLookup = NULL;

  assert(binExpectations != NULL);
  free(binExpectations);
  binExpectations = NULL;

  assert(binCounts != NULL);
  free(binCounts);
  binCounts = NULL;

  return (pValue >= 0.001);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t j;
  size_t sum;
  int opt;
  bool testResult;
  statData_t *data = NULL;
  size_t datalen;
  size_t k;
  double median;
  size_t *symbolCounts = NULL;
  unsigned long long int inint;
  char *nextOption;
  long inparam;
  struct randstate rstate;
  size_t configSubsetIndex;
  size_t configSubsetSize;
  bool configUseFile;
  size_t configRandDataSize;
  uint32_t configK;

  configSubsetIndex = 0;
  configSubsetSize = 0;
  configK = 2;
  configUseFile = true;
  configRandDataSize = 1000000;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  while ((opt = getopt(argc, argv, "vl:k:s:r")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'l':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          useageExit();
        }
        configSubsetIndex = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSubsetSize = inint;
        break;
      case 'k':
        inparam = strtol(optarg, NULL, 0);
        if ((inparam <= 0) || (inparam > 0xFFFFFFFF)) {
          useageExit();
        }
        configK = (uint32_t)inparam;
        break;
      case 'r':
        configUseFile = false;
        break;
      case 's':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 1000000000)) {
          useageExit();
        }
        configRandDataSize = (uint32_t)inparam;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (configUseFile) {
    if (argc != 1) {
      useageExit();
    }

    if ((infp = fopen(argv[0], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_NOINPUT);
    }

    datalen = readuintfileloc(infp, &data, configSubsetIndex, configSubsetSize);
    assert(data != NULL);
    assert(datalen > 0);

    if (fclose(infp) != 0) {
      perror("Couldn't close input data file");
      exit(EX_OSERR);
    }
  } else {
    seedGenerator(&rstate);

    if ((data = malloc(configRandDataSize * sizeof(statData_t))) == NULL) {
      perror("Can't allocate buffer for data");
      exit(EX_OSERR);
    }

    genRandInts(data, configRandDataSize, configK - 1, &rstate);
    k = configK;
    datalen = configRandDataSize;
  }

  translate(data, datalen, &k, &median);
  assert(k > 1);

  if (configVerbose > 0) {
    fprintf(stderr, "Testing %zu samples with %zu symbols\n", datalen, k);
  }

  if (k == 2) {
    // The one count is invariant across the tests
    sum = 0;
    for (j = 0; j < (datalen); j++) {
      sum += data[j];
    }

    testResult = binaryChiSquareIndependence(data, datalen, sum);
    printf("Binary Chi-Square Independence Test Verdict: ");
    if (testResult) {
      printf("Pass\n");
    } else {
      printf("Fail\n");
    }

    testResult = binaryChiSquareFit(data, datalen, sum);
    printf("Binary Chi-Square Goodness-of-Fit Test Verdict: ");
    if (testResult) {
      printf("Pass\n");
    } else {
      printf("Fail\n");
    }
  } else {
    if ((symbolCounts = malloc(k * sizeof(size_t))) == NULL) {
      perror("Can't allocate symbolCounts");
      exit(EX_OSERR);
    }

    for (j = 0; j < k; j++) {
      symbolCounts[j] = 0;
    }

    for (j = 0; j < datalen; j++) {
      symbolCounts[data[j]]++;
    }

    testResult = nonbinaryChiSquareIndependence(data, datalen, symbolCounts, k);
    printf("Non-binary Chi-Square Independence Test Verdict: ");
    if (testResult) {
      printf("Pass\n");
    } else {
      printf("Fail\n");
    }

    testResult = nonbinaryChiSquareFit(data, datalen, symbolCounts, k);
    printf("Non-binary Chi-Square Goodness-of-Fit Test Verdict: ");
    if (testResult) {
      printf("Pass\n");
    } else {
      printf("Fail\n");
    }

    assert(symbolCounts != NULL);
    free(symbolCounts);
    symbolCounts = NULL;
  }

  free(data);

  return EX_OK;
}
