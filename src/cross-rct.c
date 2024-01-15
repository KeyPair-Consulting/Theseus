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

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

#define INITIAL_RUN_LIMIT 128

/*
 * This program is a demonstration of the Cross RCT health test.
 * This program is both a proof-of-concept for this health test,
 * and a tool that can be used to help establish reasonable cutoffs
 * for the literal and crosswise RCTs.
 */

// CrossRCT state.
// The meaning of A, B, and C are as in SP 800-90B, Section 4.4.1.
// A is the reference symbol being compared against
// B is the number of times that the current symbol has been encountered consecutively (i.e., we have encountered a length-B run of the relevant symbol)
// C is the cutoff (the test is judged a failure if a run of C or more symbols is found)
struct crossRCTstate {
  uint8_t symbolBitWidth;
  size_t stateSize;
  size_t RCT_Input;
  size_t *RCT_C;
  uint8_t *RCT_A;
  uint8_t *RCT_X;
  size_t *RCT_B;
  size_t *RCT_Failures;
  size_t **runCounts;
  size_t *runCountLengths;
  size_t maxLiteralFailures;
  size_t maxCrossFailures;
};

/*Both index and coordinates are 0-indexed*/
/*The underlying structure is essentially the lower triangle of a square nxn matrix*/
/*The diagonal (whose corresponding cross-RCTs don't make sense) are the literal RCTs.*/
/*The non-diagonal values are the cross-RCTs*/
#if 0
/*This isn't used presently, but it could be useful...*/
static void indexToCoordinates(size_t k, uint8_t *i, uint8_t *j) {
  double di;
  assert(i!=NULL);
  assert(j!=NULL);

  di = ceil((sqrt(8.0*((double)k)+9.0)-1.0)/2.0)-1.0;
  assert(di <= UINT8_MAX);
  assert(di >= 0);
  *i = (uint8_t)di;
  *j = (uint8_t)(k - (*i)*(*i+1)/2);
}
#endif

static size_t coordinatesToIndex(uint8_t i, uint8_t j) {
  return ((size_t)i)*(((size_t)i)+1)/2 + (size_t)j;
}

static void crossExpand(uint64_t in, uint8_t n, uint8_t *X) {
  //size_t totalState = coordinatesToIndex(n-1, n-1) + 1;
  assert(n<=64);


  //for(size_t k=0; k<totalState; k++) X[k] = (uint8_t)255U;

  for(uint8_t i=0; i<n; i++) {
    size_t curIndex = coordinatesToIndex(i,i);
    //assert(X[curIndex]==255);
    X[curIndex] = (((1U<<i)&in)==0)?0:1;
    for(uint8_t j=0; j<i; j++) {
      curIndex = coordinatesToIndex(i,j);
      //assert(X[curIndex]==255);
      X[curIndex] = X[coordinatesToIndex(i,i)] ^ X[coordinatesToIndex(j,j)];
    }
  }

  //for(size_t k=0; k<totalState; k++) assert(X[k] != 255);
}

// This test is useful for RO-based designs, in particular where each RO is of the same design
// (i.e., has the same number of delay elements).
// In such cases, the nominal frequencies may be close, so there is possibly an
// entrainment problem, both at the output and per-delay element levels.
//
// This Crosswise Repetition Count Test (CRCT) takes each 64-bit symbol, and for each of the flagged lower bits, 
// treats each of the bits as parallel outputs of a noise source copy.
// There is an RCT on the literal outputs of each noise source copy, and also on the
// "crosswise" XOR of them, e.g., RO 3 with RO 7.
//
// The cross RCTs detect prolonged instances where a noise source copy pair is either persistently
// equal, or persistently complements of each other.
// In most systems, such crosswise runs are expected (simply due to the underlying deterministic
// portion of the RO behavior), but they should not persist in the long term (as the relative
// phases of the ROs are expected to independently drift).
//
// For an n-bit wide symbol, there are exactly n choose 2 "cross" patterns that we need, and n literal RCTs.

//  Set the initial cross RCT state
static void initCrossRCT(uint8_t symbolBitWidth, size_t baseRCTcutoff, size_t crossRCTcutoff, struct crossRCTstate *state) {
  assert(symbolBitWidth > 0);
  assert(symbolBitWidth <= 64);
  assert(state != NULL);

  state->stateSize = coordinatesToIndex(symbolBitWidth-1, symbolBitWidth-1) + 1;
  if(configVerbose > 0) fprintf(stderr, "Tracking a total of %zu (literal- and cross-) RCTs\n",  state->stateSize);

  state->symbolBitWidth = symbolBitWidth;
  state->RCT_Input = 0;

  state->RCT_C = calloc(state->stateSize, sizeof(size_t));
  state->RCT_B = calloc(state->stateSize, sizeof(size_t));
  state->RCT_Failures = calloc(state->stateSize, sizeof(size_t));
  state->RCT_A = calloc(state->stateSize, sizeof(uint8_t));
  state->RCT_X = calloc(state->stateSize, sizeof(uint8_t));
  state->runCounts = calloc(state->stateSize, sizeof(size_t *));
  state->runCountLengths = calloc(state->stateSize, sizeof(size_t));

  assert(state->RCT_C && state->RCT_B && state->RCT_Failures && state->RCT_A && state->RCT_X && state->runCounts && state->runCountLengths);

  for (uint8_t i = 0; i < symbolBitWidth; i++) {
    for(uint8_t j=0; j<=i; j++) {
      size_t curIndex = coordinatesToIndex(i,j);
      state->RCT_C[curIndex] = (i==j)?baseRCTcutoff:crossRCTcutoff;
      state->RCT_Failures[curIndex] = 0;
      state->runCounts[curIndex] = NULL;
      state->runCountLengths[curIndex] = 0;
    }
  }

  state->maxLiteralFailures = 0;
  state->maxCrossFailures = 0;
}

#define UNALLOC(X) do { if(X) {free(X); (X)=NULL;} } while(0)

static void unallocCrossRCT(struct crossRCTstate *state) {
  UNALLOC(state->RCT_C);
  UNALLOC(state->RCT_B);
  UNALLOC(state->RCT_Failures);
  UNALLOC(state->RCT_A);
  UNALLOC(state->RCT_X);
  UNALLOC(state->runCounts);
  UNALLOC(state->runCountLengths);
}

// A function that rounds up to the next power of 2.
// Used to establish the size of memory allocation.
static size_t pow2Round(size_t x) {
  assert(x>0);
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  x++;
  assert(x>0);
  return x;
}

// To understand the logic of the RCT, it is useful to refer to
// SP 800-90B, Section 4.4.1. The variable names A, B, C all are consistent with that document.
// The "X" of SP 800-90B is one of the bits of the variable "base", which is either one of the bits of "in",
// or one of the crosswise XOR values.
static bool crossRCT(uint64_t in, struct crossRCTstate *state) {
  bool RCT_Pass = true;
  size_t literalFailures = 0;
  size_t crossFailures = 0;

  assert(state != NULL);

  //Step 3 of the CRCT health test
  // X = crossExpand(next())
  crossExpand(in, state->symbolBitWidth, state->RCT_X);

  if (state->RCT_Input == 0) {
    // Step 2 of the CRCT health test
    for(size_t i=0; i<state->stateSize; i++) {
      state->RCT_B[i] = 1;
      if (state->runCounts[i]) state->runCounts[i][1] = 1;
    }
  } else {
    for (uint8_t i = 0; i < state->symbolBitWidth; i++) { // loop in step 4 of the CRCT health test.
      for(uint8_t j = 0; j<=i; j++) {
        size_t curIndex = coordinatesToIndex(i,j);
        if (state->RCT_X[curIndex] != state->RCT_A[curIndex]) { //step 4a
          //Step 4b(i) of the CRCT health test
          // Reset the run count for this test.
          state->RCT_B[curIndex] = 1;
        } else {
          // The two bits are the same, so we extend the current run.
          //Step 4a(i) in the CRCT health test
          state->RCT_B[curIndex]++;

          // If this would be flagged as an error, update the error counter.
          // This is step 4a(ii); in this context, we are only counting the total number of failures.
          if (state->RCT_B[curIndex] >= state->RCT_C[curIndex]) {
            state->RCT_Failures[curIndex]++;
            RCT_Pass = false;
            if(i==j) literalFailures++;
            else crossFailures++;
          }
        }

        // This allows us to keep track of the per-symbol run length distribution, which is helpful for establishing the cutoff.
        // Record the run length (if we are doing that)
        if (state->runCounts[curIndex]) {
          if(state->runCountLengths[curIndex] <= state->RCT_B[curIndex]) {
            size_t newLength = pow2Round(state->RCT_B[curIndex]+1);
            size_t oldLength = state->runCountLengths[curIndex];
            if(configVerbose > 2) fprintf(stderr, "Reallocate for bin %u: %zu -> %zu\n", j, oldLength, newLength);
            state->runCounts[curIndex] = realloc(state->runCounts[curIndex], sizeof(size_t)*newLength);
            assert(state->runCounts[curIndex]!=NULL);

            //Clear out the new memory.
            for(size_t index=oldLength; index<newLength; index++) state->runCounts[curIndex][index] = 0;

            //Note that we have more memory now...
            state->runCountLengths[curIndex] = newLength;
          }
          state->runCounts[curIndex][state->RCT_B[curIndex]]++;
        }
      }
    }
  }

  if(state->maxLiteralFailures < literalFailures) state->maxLiteralFailures = literalFailures;
  if(state->maxCrossFailures < crossFailures) state->maxCrossFailures = crossFailures;

  // Step 5 (or step 1 for the first symbol) of the CRCT health test.
  // Update the reference data for all tests.
  for(size_t i=0; i<state->stateSize; i++) {
    state->RCT_A[i] = state->RCT_X[i];
  }

  // Note that we processed an additional input.
  state->RCT_Input++;

  return RCT_Pass;
}

/*Count the number of bits required to represent the total number of symbols.
 * This essentially just forces all bits lower than the msb to be 1, and then
 * counts the number of 1s present.
 */
static uint8_t bitsRequired(uint64_t x) {
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  return (uint8_t)__builtin_popcountll(x);
}

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "cross-rct [-v] [-c <value>] [-C <value>] [-d <value>] [-t <value>] <infile>\n");
  fprintf(stderr, "Runs the SP 800-90B RCT health test on provided values that are expected to be simultaneously sampled RO copies that need to be checked for entrainment.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-C <value>\tThe RCT \"crosswise\" (i.e., XOR of two distinct RO values) cutoff value is <value>.\n");
  fprintf(stderr, "-c <value>\tThe RCT \"literal\" (i.e., the literal output of one of the RO copies) cutoff value is <value>.\n");
  fprintf(stderr, "-t <value>\tTry to find suggested cutoff values, if the desired (per-symbol) false positive rate is 2^-<value>.\n");
  fprintf(stderr, "-d <value>\tData is presumed to be <value>-bit wide symbols. (supported values are 8, 32, and 64-bits).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint64_t *data = NULL;
  size_t configRCTC_literal;
  size_t configRCTC_cross;
  int opt;
  unsigned long long inint;
  struct crossRCTstate healthTests;
  bool configSuggestCutoffs = false;
  uint32_t configAlphaExp = 0;
  size_t maxRunCountLength = 0;
  size_t maxRunLength = 0;
  size_t *consolidatedLiteralRunCounts = NULL;
  size_t *consolidatedCrosswiseRunCounts = NULL;
  size_t totalFailures = 0;
  uint32_t configBitWidth = 0;
  statData_t *u8Data = NULL;
  uint32_t *u32Data = NULL;
  uint8_t bitsInUse = 0;
  uint64_t orMask=0;


  configVerbose = 0;
  configRCTC_literal = 0;
  configRCTC_cross = 0;

  while ((opt = getopt(argc, argv, "vC:c:t:d:")) != -1) {
    switch (opt) {
      case 'v':
        // Output more debug information.
        configVerbose++;
        break;
      case 'C':
        // Set the cross RCT bound.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configRCTC_cross = (size_t)inint;
        break;
      case 'c':
        // Set the literal RCT bound.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configRCTC_literal = (size_t)inint;
        break;
      case 't':
        // Estimate the appropriate cutoffs.
        configSuggestCutoffs = true;

        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAlphaExp = (uint32_t)inint;
        if ((configAlphaExp < 20) || (configAlphaExp > 40)) fprintf(stderr, "Desired alphaExp of %u is outside of the recommended interval [20, 40].\n", configAlphaExp);
        break;
      case 'd':
        inint = strtoull(optarg, NULL, 0);
        if ((errno == EINVAL) || ((inint != 8) && (inint != 32) &&(inint != 64))) {
          useageExit();
        }
        configBitWidth = (uint32_t)inint;
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

  if(configVerbose > 0) {
    if(configRCTC_cross > 0) printf("Cross RCT cross cutoff: %zu\n", configRCTC_cross);
    if(configRCTC_literal > 0) printf("Cross RCT literal cutoff: %zu\n", configRCTC_literal);
  }

  if(configBitWidth == 0) {
      char *suffix;
      size_t fileNameLen = strlen(argv[0]);

      for(suffix = argv[0] + (fileNameLen - 1); (suffix > argv[0]) && (*suffix != '-'); suffix--);
      if(suffix != argv[0]) {
        if(strcmp(suffix, "-u64.bin")==0) {
          configBitWidth = 64;
        } else if(strcmp(suffix, "-u32.bin")==0) {
          configBitWidth = 32;
        } else if(strcmp(suffix, "-u8.bin")==0) {
          configBitWidth = 8;
        } else if(strcmp(suffix, "-sd.bin")==0) {
          configBitWidth = STATDATA_BITS;
        }
    }
  }

  if(configBitWidth == 0) {
    fprintf(stderr, "No sample bit width was specified, and it can't be guessed.\n");
    useageExit();
  }

  if(configVerbose > 0) fprintf(stderr, "Using %u bit symbols.\n", configBitWidth);

  // Read in the data
  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if(configBitWidth == 8) {
    datalen = readuintfile(infp, &u8Data);
    assert(u8Data != NULL);
    assert(datalen > 0);
    data = calloc(datalen, sizeof(uint64_t));
    assert(data!=NULL);
    for(size_t j=0; j<datalen; j++) data[j] = u8Data[j];
    UNALLOC(u8Data);
  } else if(configBitWidth == 32) {
    datalen = readuint32file(infp, &u32Data);
    assert(u32Data != NULL);
    assert(datalen > 0);
    data = calloc(datalen, sizeof(uint64_t));
    assert(data!=NULL);
    for(size_t j=0; j<datalen; j++) data[j] = u32Data[j];
    UNALLOC(u32Data);
  } else if(configBitWidth == 64) {
    datalen = readuint64file(infp, &data);
    assert(data != NULL);
    assert(datalen > 0);
  } else {
    useageExit();
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }

  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  // check for the actual bit width.
  for(size_t i=0; i<datalen; i++) {
    orMask |= data[i];
  }
  bitsInUse = bitsRequired(orMask); 

  if(configVerbose > 0) fprintf(stderr, "Found %u active bits.\n", bitsInUse);

  initCrossRCT(bitsInUse, configRCTC_literal, configRCTC_cross, &healthTests);

  // If configVerbose > 1 (or if we were asked to estimate cutoffs), then we'll keep track of the transitions.
  // This functionality is mainly useful to establish appropriate cutoff settings,
  // and would not generally be present in a deployed entropy source.
  if (configSuggestCutoffs || (configVerbose > 1)) {
    for (unsigned int j = 0; j < healthTests.stateSize; j++) {
      healthTests.runCounts[j] = calloc(INITIAL_RUN_LIMIT, sizeof(size_t));
      assert(healthTests.runCounts[j] != NULL);
      healthTests.runCountLengths[j] = INITIAL_RUN_LIMIT;
    }
  }

  // Feed in all the data to the RCT test.
  for (size_t i = 0; i < datalen; i++) {
    if(!crossRCT(data[i], &healthTests)) totalFailures++;
  }

  // Report on the results of the test.
  // For the consolidated run list
  if (configSuggestCutoffs) {
    maxRunCountLength = 0;
    for(size_t j=0; j<healthTests.stateSize; j++) {
      if(maxRunCountLength < healthTests.runCountLengths[j]) maxRunCountLength = healthTests.runCountLengths[j];
    }

    consolidatedLiteralRunCounts = calloc(maxRunCountLength, sizeof(size_t));
    assert(consolidatedLiteralRunCounts != NULL);
    consolidatedCrosswiseRunCounts = calloc(maxRunCountLength, sizeof(size_t));
    assert(consolidatedCrosswiseRunCounts != NULL);
  }

  // Convention: The RCTs are all conceptually stored in a lower-triangular matrix whose element indices start at 0
  // The diagonal entries are the literal RCTs, and the sub-diagonal elements are the cross RCTs.
  for(uint8_t i=0; i<healthTests.symbolBitWidth; i++) {
    for(uint8_t j=0; j<=i; j++) {
      char label[20];
      size_t curIndex = coordinatesToIndex(i, j);
      size_t RCT_Count;

      if (i==j) {
        sprintf(label, "Literal osc %u", i);
      } else {
        sprintf(label, "XOR of osc (%u, %u)", i, j);
      } 

      if((healthTests.RCT_C[curIndex] > 0) || (configVerbose > 1)) printf("RCT %s: ", label);

      assert(healthTests.RCT_Input >= healthTests.RCT_C[curIndex]);
      RCT_Count = healthTests.RCT_Input - healthTests.RCT_C[curIndex] + 1;

      if(healthTests.RCT_C[curIndex] > 0) {
        // We were asked to report on this RCT and the failure rates might mean something.
        printf("Failure Rate: %zu / %zu = %g", healthTests.RCT_Failures[curIndex], RCT_Count, ((double)(healthTests.RCT_Failures[curIndex])) / ((double)RCT_Count));
        if (healthTests.RCT_Failures[curIndex] > 0) {
          printf(" ≈ 2^%g\n", log2(((double)(healthTests.RCT_Failures[curIndex])) / ((double)RCT_Count)));
        } else {
          printf("\n");
        }
      }

      // If we've been keeping count of the run lengths, print it out.
      if (healthTests.runCounts[curIndex] != NULL) {
        char startChar = '{';
        for (size_t k = 0; k < healthTests.runCountLengths[curIndex]; k++) {
          // Save the length of the largest run.
          if ((healthTests.runCounts[curIndex])[k] > 0) {
            if (k > maxRunLength) maxRunLength = k;

            if (configSuggestCutoffs) {
              if (i==j)
                consolidatedLiteralRunCounts[k] += (healthTests.runCounts[curIndex])[k];
              else
                consolidatedCrosswiseRunCounts[k] += (healthTests.runCounts[curIndex])[k];
            }
          }

          if ((configVerbose > 1) && ((healthTests.runCounts[curIndex])[k] != 0)) {
            printf("%c {%zu, %zu}", startChar, k, healthTests.runCounts[curIndex][k]);
            startChar = ',';
          }
        }
        if (configVerbose > 1) printf("};\n");
      }
    }
  }

  if(maxRunLength > 0) printf("Longest encountered run: %zu\n", maxRunLength);
  if(configRCTC_literal > 0) {
    if(healthTests.maxLiteralFailures > 0) printf("Most simultaneous literal failures: %zu\n", healthTests.maxLiteralFailures);
    else printf("No recorded literal failures.\n");
  }

  if(configRCTC_cross > 0) {
    if(healthTests.maxCrossFailures > 0) printf("Most simultaneous cross failures: %zu\n", healthTests.maxCrossFailures);
    else printf("No recorded cross failures.\n");
  }

  if (configSuggestCutoffs) {
    // Just add to either the crosswise or literal bucket list.
    long double alpha = powl(2.0L, -(long double)configAlphaExp);
    // There are a total of healthTests.stateSize distinct tests that we are treating as independent.
    long double perTestAlpha = 1.0L - powl(1.0L - alpha, 1.0L / ((long double)healthTests.stateSize));
    size_t allowedLiteralFailures = (size_t)floorl(8.0L * ((long double)datalen) * perTestAlpha);
    size_t allowedCrosswiseFailures = (size_t)floorl(28.0L * ((long double)datalen) * perTestAlpha);
    size_t accumulatedFailures;

    if (configVerbose > 0) fprintf(stderr, "Allowed consolidated literal failures: %zu\nAllowed consolidated crosswise failures: %zu\n", allowedLiteralFailures, allowedCrosswiseFailures);

    if (allowedLiteralFailures < 10) fprintf(stderr, "The bound for the desired number of consolidated literal failures (%zu < 10) may not yield statistically meaningful results.\n", allowedLiteralFailures);
    if (allowedCrosswiseFailures < 10) fprintf(stderr, "The bound for the desired number of consolidated crosswise failures (%zu < 10) may not yield statistically meaningful results.\n", allowedCrosswiseFailures);

    if (configVerbose > 0) {
      fprintf(stderr, "Targeted per-test alpha is approximately 2^%0.22Lg\n", log2l(perTestAlpha));
    }

    accumulatedFailures = 0;
    for (size_t j = maxRunLength; j > 0; j--) {
      accumulatedFailures += consolidatedLiteralRunCounts[j];
      if (accumulatedFailures > allowedLiteralFailures) {
        printf("Empirical literal cutoff: %zu\n", j + 1);
        break;
      }
    }

    accumulatedFailures = 0;
    for (size_t j = maxRunLength; j > 0; j--) {
      accumulatedFailures += consolidatedCrosswiseRunCounts[j];
      if (accumulatedFailures > allowedCrosswiseFailures) {
        printf("Empirical crosswise cutoff: %zu\n", j + 1);
        break;
      }
    }
  }

  if((configRCTC_literal > 0) && (configRCTC_cross > 0)) {
    printf("Overall test failure rate: %zu / %zu = %g", totalFailures, datalen, ((double)totalFailures) / ((double)datalen));
    if (totalFailures > 0) {
      printf(" ≈ 2^%g\n", log2(((double)totalFailures) / ((double)datalen)));
    } else {
      printf("\n");
    }
  }

  // Free all the allocated memory.
  for (unsigned int j = 0; j < healthTests.stateSize; j++) {
    if (healthTests.runCounts[j] != NULL) {
      UNALLOC(healthTests.runCounts[j]);
    }
  }

  if (configSuggestCutoffs) {
    UNALLOC(consolidatedLiteralRunCounts);
    UNALLOC(consolidatedCrosswiseRunCounts);
  }

  unallocCrossRCT(&healthTests);
  UNALLOC(data);
  return EX_OK;
}
