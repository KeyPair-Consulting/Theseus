/* This file is part of the Theseus distribution.
 * Copyright 2023 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "globals-inst.h"
#include "randlib.h"
#include "precision.h"

/*
 * This program is a demonstration of the APT health test.
 * This program is both a proof-of-concept for this health test,
 * and a tool that can be used to help establish reasonable cutoffs
 * for the APT.
 */

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "apt-sim [-v] [-w <value>] [-t <value>] [-r <rounds>] <min ent>\n");
  fprintf(stderr, "Simulates the \"worst case\" distribution underlying the SP 800-90B APT health test with the provided parameters.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-w <value>\tThe APT window value is <value>. (Default is 1024 symbols when <min ent> <= 1.0, and 512 otherwise.)\n");
  fprintf(stderr, "-t <value>\tTry to find suggested cutoff values, if the desired (per-window) false positive rate is 2^-<value>.\n");
  fprintf(stderr, "-r <value>\tUse <value> rounds of simulation.\n");
  exit(EX_USAGE);
}

/* This is xoshiro256** 1.0*/
/*This implementation is derived from David Blackman and Sebastiano Vigna's implementation,
 *which they placed into the public domain.
 *See http://xoshiro.di.unimi.it/xoshiro256starstar.c
 */
static inline uint64_t rotl(const uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}

static inline uint64_t xoshiro256starstar(uint64_t *xoshiro256starstarState) {
  const uint64_t result_starstar = rotl(xoshiro256starstarState[1] * 5, 7) * 9;
  const uint64_t t = xoshiro256starstarState[1] << 17;

  xoshiro256starstarState[2] ^= xoshiro256starstarState[0];
  xoshiro256starstarState[3] ^= xoshiro256starstarState[1];
  xoshiro256starstarState[1] ^= xoshiro256starstarState[2];
  xoshiro256starstarState[0] ^= xoshiro256starstarState[3];

  xoshiro256starstarState[2] ^= t;

  xoshiro256starstarState[3] = rotl(xoshiro256starstarState[3], 45);

  return result_starstar;
}

/*Produces a double with the full significand worth of randomness, where "adjacent" possible values are evenly spaced, in the interval [0,1)*/
inline static double fastRandomUnit(uint64_t *xoshiro256starstarState) {
  /*Note that 2^53 is the largest integer that can be represented in a 64 bit IEEE 754 double, such that all smaller positive integers can also be
   * represented. Anding by 0x001FFFFFFFFFFFFFULL masks out the lower 53 bits, so the resulting integer is in the range [0, 2^53 - 1].
   * 0x1.0p-53 is 2^(-53) (multiplying by this value just affects the exponent of the resulting double, not the significand)!
   * We get a double uniformly distributed in the range [0, 1).  (The delta between adjacent values is 2^(-53))
   * There is a lot of misinformation on the internet on how to do this well; this is the approach that yields the most possible distinct outputs
   * that are evenly spaced, and is basically the same as used by the SFMT (once it was corrected) and xoroshiro authors.
   */
  return (((double)(xoshiro256starstar(xoshiro256starstarState) & 0x001FFFFFFFFFFFFFULL)) * 0x1.0p-53);
}

// Here, we simulate a "worst case" distribution for the APT, in the sense that the adopted distribution
// results in the largest acceptable collision bound for a given assessed entropy level, so if a data sample fails a test
// whose cutoff was derived from this simulation, it is likely to indicate an underlying problem.
//
// This "worst case" uses the "inverted near-uniform" family (see Hagerty-Draper "Entropy Bounds and Statistical Tests" for
// a full definition of this distribution and justification for its use here).
//
// This distribution has as many maximal probability symbols as possible (each occurring with probability p), and possibly one
// additional symbol that contains all the residual probability.
//
// If the probability for the most likely symbol is p, then there are floor(1/p) most likely symbols,
// each occurring with probability p and possibly one additional symbol that has all the remaining (1 - p floor(1/p)) chance.
// In this code, we generate a random unit value in the range [0, 1), and we need to map this to one of the ceil(1/p) possible
// output symbols.
//
// Note that the function x -> floor(x/p) yields
// [0p,1p) -> 0
// [1p, 2p) -> 1
// [2p, 3p) -> 2
// ...
// [(floor(1/p)-1)p, floor(1/p)p) -> floor(1/p)-1
// [ floor(1/p)p, 1 ) -> floor(1/p)
//
// As such, each of the first floor(1/p) symbols (0 through floor(1/p)-1) have probability p of occurring, and
// the symbol floor(1/p) has probability 1-floor(1/p)p of occurring, as desired.
//
// Note that if floor(1/p) = ceil(1/p) = 1/p, then there is no "residual" symbol, only 1/p most likely symbols.
//
// The array is 0-indexed, so we can use this map to establish the index directly.
static size_t simulateCount(double p, size_t W, uint64_t *seed) {
  size_t refData = (size_t)floor(fastRandomUnit(seed) / p);
  size_t observedCount = 1;

  for (size_t j = 1; j < W; j++) {
    if (((size_t)floor(fastRandomUnit(seed) / p)) == refData) observedCount++;
  }

  return observedCount;
}

// This returns the bound (cutoff) for the test. Counts equal to this value should pass.
// Larger values should fail.
static void simulateBound(long double alpha, double H, size_t W, size_t simulation_rounds) {
  size_t *results;
  double p;

  assert(W > 0);
  assert(H > 0);

  if ((results = calloc(W + 1, sizeof(size_t))) == NULL) {
    fprintf(stderr, "Can't allocate array for tracking APT results.\n");
    exit(EX_OSERR);
  }

  // The probability of the most likely symbol (MLS) only needs to be calculated once...
  p = pow(2.0, -H);

#pragma omp parallel
  {
    struct randstate rstate;
    size_t *localResults;
    uint64_t seed[4];

    initGenerator(&rstate);
    seedGenerator(&rstate);
    for(int j=0; j<4; j++) seed[j] = randomu64(&rstate);

    if ((localResults = calloc(W + 1, sizeof(size_t))) == NULL) {
      fprintf(stderr, "Can't allocate array for tracking APT results.\n");
      exit(EX_OSERR);
    }

#pragma omp for
    for (size_t i = 0; i < simulation_rounds; i++) {
      localResults[simulateCount(p, W, seed)]++;
    }

#pragma omp critical(resultUpdate)
    {
      for (size_t i = 0; i <= W; i++) results[i] += localResults[i];
    }

    free(localResults);
  }

  if (configVerbose > 0) {
    char startChar = '{';
    fprintf(stderr, "Encountered counts: ");
    for (size_t i = 0; i <= W; i++) {
      if (results[i] > 0) {
        fprintf(stderr, "%c {%zu, %zu}", startChar, i, results[i]);
        startChar = ',';
      }
    }
    fprintf(stderr, "};\n");
  }

  if(alpha > 0.0L) {
    // Find the cutoff
    size_t allowedFailures = (size_t)floorl(((long double)simulation_rounds) * alpha);
    size_t accumulatedFailures;

    if (configVerbose > 0) fprintf(stderr, "Allowed failures: %zu\n", allowedFailures);

    if (allowedFailures < 10) fprintf(stderr, "The bound for the desired number of failures (%zu < 10) may not yield statistically meaningful results.\n", allowedFailures);

    accumulatedFailures = 0;

    for (size_t j = W; j > 0; j--) {
      accumulatedFailures += results[j];
      if (accumulatedFailures > allowedFailures) {
        printf("Empirical APT cutoff: %zu\n", j + 1);
        break;
      }
    }
  }

  free(results);
}

int main(int argc, char *argv[]) {
  size_t configAPTW=0;
  bool defaultWindow = true;
  int opt;
  unsigned long long inint;
  uint32_t configAlphaExp = 0;
  char *endptr;
  double configH;
  size_t configRounds = 1000000;
  long double alpha=0.0L;

  configVerbose = 0;

  while ((opt = getopt(argc, argv, "vw:t:r:")) != -1) {
    switch (opt) {
      case 'v':
        // Output more debug information.
        configVerbose++;
        break;
      case 'w':
        // Set the APT window.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAPTW = (size_t)inint;
        defaultWindow = false;
        break;
      case 'r':
        // Set the number of simulation rounds.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configRounds = (size_t)inint;
        break;
      case 't':
        // Estimate the appropriate cutoffs.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAlphaExp = (uint32_t)inint;
        if ((configAlphaExp < 20) || (configAlphaExp > 40)) fprintf(stderr, "Desired alphaExp of %u is outside of the recommended interval [20, 40].\n", configAlphaExp);

        alpha = powl(2.0L, -((long double)configAlphaExp));
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

  configH = strtod(argv[0], &endptr);
  if((endptr == argv[0]) || (*endptr != '\0') || (configH <= 0.0)) {
    useageExit();
  }

  if(defaultWindow) {
    if(configH <= 1.0) configAPTW = 1024;
    else configAPTW = 512;
  }

  if (configVerbose > 0) {
    printf("APT Window Size: %zu\n", configAPTW);
  }

  simulateBound(alpha, configH, configAPTW, configRounds);

  return EX_OK;
}
