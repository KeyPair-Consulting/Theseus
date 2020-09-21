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
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "fancymath.h"
#include "globals-inst.h"
#include "randlib.h"
#include "sa.h"
#include "translate.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "lrs-test [-v] [-l <index>,<samples> ] inputfile\nor\n");
  fprintf(stderr, "lrs-test -r [-k <m>] [-s <m>]\n");
  fprintf(stderr, "inputfile is assumed to be a sequence of" STATDATA_STRING "s\n");
  fprintf(stderr, "-v\tVerbose mode.\n");
  fprintf(stderr, "-l <index>,<samples>\tRead the <index> substring of length <samples>.\n");
  fprintf(stderr, "-r \t instead of doing testing on provided data use a random iid variable.\n");
  fprintf(stderr, "-k <m> \t Use an alphabet of <m> values. (default k=2)\n");
  fprintf(stderr, "-s <m> \t Use a sample set of <m> values. (default m=1000000)\n");

  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t L;
  statData_t *data = NULL;
  size_t j;
  SAINDEX W;
  SAINDEX *SA, *LCP;
  SAINDEX loc;
  size_t *dataCount;
  long double p_col;
  int64_t N;
  size_t k;
  double median;
  size_t configSubsetIndex;
  size_t configSubsetSize;
  bool configUseFile;
  size_t configRandDataSize;
  size_t configK;
  unsigned long long int inint;
  long inparam;
  char *nextOption;
  int opt;
  struct randstate rstate;

  long double p_colPower;
  long double logProbNoColsPerPair;
  long double probAtLeastOneCols;
  long double probNoCols;
  bool verdict;
  bool roundedVerdict;

  configSubsetIndex = 0;
  configSubsetSize = 0;
  configK = 2;
  configUseFile = true;
  configRandDataSize = 1000000;

  initGenerator(&rstate);

  while ((opt = getopt(argc, argv, "vl:rk:s:")) != -1) {
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
        configK = (size_t)inparam;
        break;
      case 'r':
        configUseFile = false;
        break;
      case 's':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 1000000000)) {
          useageExit();
        }
        configRandDataSize = (size_t)inparam;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  assert(configK > 0);
  assert(configK - 1 < UINT32_MAX);

  if (configUseFile) {
    if (argc != 1) {
      useageExit();
    }
    if ((infp = fopen(argv[0], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_NOINPUT);
    }

    L = readuintfileloc(infp, &data, configSubsetIndex, configSubsetSize);
    assert(data != NULL);
    assert(L > 0);

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

    genRandInts(data, configRandDataSize, (uint32_t)(configK - 1), &rstate);
    k = configK;
    L = configRandDataSize;
  }

  translate(data, L, &k, &median);
  assert(k > 1);
  assert(L>k);

  if (configVerbose > 0) {
    fprintf(stderr, "Testing %zu samples with %zu symbols\n", L, k);
  }

  if ((dataCount = (size_t *)malloc(k * sizeof(size_t))) == NULL) {
    perror("Cannot allocate memory.\n");
    exit(EX_OSERR);
  }

  for (j = 0; j < k; j++) {
    dataCount[j] = 0;
  }

  for (j = 0; j < L; j++) {
    dataCount[data[j]]++;
  }

  // We are looking for some per-symbol collision probability. This match could be any particular symbol,
  // so for each symbol with probability p_i, the probability of that symbol reoccurring (under an IID assumption) is
  //(p_i)^2. The collision could be for any symbol, so add these squares.
  // This is just the summand part of the collision entropy calculation!
  // p_col is the probability of collision on a per-symbol basis under an IID assumption (this is related to the collision entropy).
  // Note: We are adding at most 256 non-negative values, so the errors are not likely to accumulate too much when using long double.
  // Note: p_col >= 1/k; for SP 800-90B k<=256, so we can bound p_col >= 2^-8.

  p_col = 0.0L;
  for (j = 0; j < k; j++) {
    long double psymbolcols = ((long double)dataCount[j]) / ((long double)L);
    p_col += psymbolcols * psymbolcols;
  }

  if(configVerbose > 0) fprintf(stderr, "p_col = %.22Lg\n", p_col);
  // It is possible for p_col to be exactly 1 (e.g., if the input data is all one symbol)
  // In this instance, a collision of any length up to L-1 has probability 1.
  if(p_col > 1.0L - LDBL_EPSILON) {
    if(configVerbose > 0) fprintf(stderr, "Pr(X>=1) = 1.0\n");
    printf("Collisions necessarily occur.\n");
    printf("LRS Test Verdict: Pass\n");
    free(data); 
    free(dataCount);
    return EX_OK;
  }

  assert(p_col < 1.0L);
  assert(p_col >= 1.0L / ((long double)k));

  assert(dataCount != NULL);
  free(dataCount);
  dataCount = NULL;

  /* Allocate 9n bytes of memory. */
  SA = (SAINDEX *)malloc((size_t)(L + 1) * sizeof(SAINDEX));  // +1 for computing LCP
  LCP = (SAINDEX *)malloc((size_t)(L + 1) * sizeof(SAINDEX));
  if ((SA == NULL) || (LCP == NULL)) {
    perror("Cannot allocate memory.\n");
    exit(EX_OSERR);
  }

  calcSALCP(data, L, k, SA, LCP);

  // Now, look for the LRS.
  W = 0;
  loc = 0;
  for (SAINDEX i = 1; i <= (SAINDEX)L; i++) {
    if (W < LCP[i]) {
      W = LCP[i];
      loc = i;
    }
  }

  // Note, W is necessarily positive (so long as L>k), and at most L-1
  assert(W > 0);
  // Note, this also assures that loc>=1.
  assert((size_t)W < L);
  if(configVerbose > 0) fprintf(stderr, "Longest repeated substring length: %d\n", W);

  if (configVerbose > 0) {
    fprintf(stderr, "Repeated string at positions %d and %d\n", SA[loc - 1], SA[loc]);
  }

  /* Deallocate memory. */
  assert(SA != NULL);
  free(SA);
  SA = NULL;
  assert(LCP != NULL);
  free(LCP);
  LCP = NULL;
  assert(data != NULL);
  free(data);
  data = NULL;

  // p_col^W is the probability of collision of a W-length string under an IID assumption;
  // this may be quite close to 0.
  // We know that p_col >= 2^-8, but if W is sufficiently large, then the result will be smaller than LDBL_MIN
  // (2^-16382 on modern Intel platforms); if this happens then we can't represent p_col^W directly as a long double.
  // There isn't anything we can do about this error condition without moving to an arbitrary precision calculation, so
  // for now, we'll just detect this condition and abort the calculation.
  p_colPower = powl(p_col, (long double)W);
  if(configVerbose > 0) fprintf(stderr, "p_col^W = %.22Lg\n", p_colPower);
  assert(p_colPower >= LDBL_MIN);
  assert(p_colPower <= 1.0L-LDBL_EPSILON);

  // There is some delicacy in calculating Pr(X>=1) as some of the intermediary values may be quite close to 0 or 1.
  // We first want to calculate the probability of not having a collision of length W for a single pair of independent
  // W-length strings.  This quantity is (1-p_col^W).
  // We know that p_col >= 2^-8, but if W is sufficiently large, then the result will be smaller than
  // LDBL_EPSILON (2^-63 on modern Intel platforms); if this happens then we can't represent (1-p_col^W) directly as a long double.
  // We instead calculate the log of the probability of not having a collision of length W for a single pair of independent W-length strings.
  // Recall that log1p(x) = log(1+x); this form is useful when |x| is small. We are particularly concerned with the case where p_col^W is a small
  // positive value, which would make log1p(-p_col^W) a negative value quite close to 0.
  logProbNoColsPerPair = log1pl(-p_colPower);
  if(configVerbose > 0) fprintf(stderr, "log(1-p_col^W)= %.22Lg\n", logProbNoColsPerPair);
  assert(logProbNoColsPerPair < 0.0L);

  //(L - W + 1) is the number of overlapping contiguous substrings of length W in a string of length L.
  // The number of pairs of such overlapping substrings is N = (L - W + 1) choose 2.
  // This is the number of ways of choosing 2 substrings of length W from a string of length L.
  N = ((int64_t)L - W + 1) * ((int64_t)L - W) / 2;
  if(configVerbose > 0) fprintf(stderr, "N = %zu\n", N);

  // Calculate the probability of not encountering a collision after N sets of independent pairs;
  // this is an application of the Binomial Distribution.
  // Using the CDF at X=0 (or equivalently, the PDF at X=0), we find the probability of seeing at least 1 collision,
  // under the assumption that each substring is independent of every other substring (i.e., under the per-round independence
  // assumption required by the Binomial Distribution), is:
  // probNoCols = 1 - Pr (X = 0) = 1 - (1 - p_col^W)^N.
  // Unfortunately, if W>1, many of these substrings are not independent! In the case that two strings overlap, then
  // there is clearly a dependency between the trials. As such, this test has some statistical construction problems.
  // Empirical testing has shown that, for ideal looking data, the observed failure rate is under the desired
  // threshold of 1/1000.
  // Note, this N is O(L^2), so use of this value as an exponent tends to cause underflows here;
  // in this case, this probability isn't accurately representable using the precision that we have to work with, but it is expected to
  // round reasonably.
  probNoCols = expl(((long double)N)*logProbNoColsPerPair);
  probAtLeastOneCols = 1.0L - probNoCols;
  roundedVerdict = (probAtLeastOneCols >= 0.001L);
  if(configVerbose > 0) {
    if((probNoCols <= 1.0L - LDBL_EPSILON) && (probNoCols >= LDBL_EPSILON)) {
      fprintf(stderr, "Pr(X >= 1) = %.22Lg\n", probAtLeastOneCols);
    } else {
      fprintf(stderr, "Pr(X >= 1) rounds to %.22Lg, but there is precision loss. The test verdict is still expected to be valid.\n", probAtLeastOneCols);
    }
  }

  //We don't need the above to have worked to come to a conclusion on the test, however:
  // The LRS test is considered a "Pass"
  // iff Pr(X>=1) >= 1/1000
  // iff 1 - Pr(X=0) >= 1/1000
  // iff 1 - (1-p_col^W)^N >= 1/1000
  // iff 0.999 >= (1-p_col^W)^N
  // iff log(0.999) >= N*log(1-p_col^W)
  // iff log(0.999) >= N*log1p(-p_col^W)
  verdict=(logl(0.999L) >= ((long double)N)* logProbNoColsPerPair);
  assert(roundedVerdict == verdict);

  if(verdict) {
    printf("LRS Test Verdict: Pass\n");
  } else {
    printf("LRS Test Verdict: Fail\n");
  }

  return EX_OK;
}
