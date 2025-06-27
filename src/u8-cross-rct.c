/* This file is part of the Theseus distribution.
 * Copyright 2023-2024 Joshua E. Hill <josh@keypair.us>
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "health-tests.h"

/*
 * This program is a demonstration of the Cross RCT health test.
 * This program is both a proof-of-concept for this health test,
 * and a tool that can be used to help establish reasonable cutoffs
 * for the literal and crosswise RCTs.
 */

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u8-cross-rct [-v] -r <value> -R <value> <infile>\n");
  fprintf(stderr, "Runs the SP 800-90B RCT health test on uint8_t values that are expected to be simultaneously sampled RO copies that need to be checked for entrainment.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-C <value>\tThe RCT \"crosswise\" (i.e., XOR of two distinct RO values) cutoff value is <value>.\n");
  fprintf(stderr, "-c <value>\tThe RCT \"literal\" (i.e., the literal output of one of the RO copies) cutoff value is <value>.\n");
  fprintf(stderr, "-t <value>\tTry to find suggested cutoff values, if the desired (per-symbol) false positive rate is 2^-<value>.\n");
  exit(EX_USAGE);
}

#define MAX_CROSSWISE_PAIR_LABEL 64
#define INITIAL_RUN_LIMIT 128

static uint8_t bitIndexToCanonicalLabel(uint8_t bitIndex) {
  uint8_t shiftByteIndex = bitIndex >> 3;
  uint8_t bitloc = bitIndex & 0x07;

  if (shiftByteIndex <= bitloc)
    return bitIndex;
  else
    return (uint8_t)((bitloc << 3U) | shiftByteIndex);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *data = NULL;
  size_t configRCTC_literal;
  size_t configRCTC_cross;
  int opt;
  unsigned long long inint;
  struct crossRCTstate healthTests;
  size_t RCT_Count;
  uint8_t outputPair[MAX_CROSSWISE_PAIR_LABEL];  // Note, the index varies across all 6-bit values
  uint8_t maxShiftIndex = 0;
  bool configSuggestCutoffs = false;
  uint32_t configAlphaExp = 0;
  size_t maxRunCountLength = 0;
  size_t maxRunLength = 0;
  size_t *consolidatedLiteralRunCounts = NULL;
  size_t *consolidatedCrosswiseRunCounts = NULL;
  size_t totalFailures = 0;

  configVerbose = 0;
  configRCTC_literal = 0;
  configRCTC_cross = 0;

  while ((opt = getopt(argc, argv, "fvC:c:t:")) != -1) {
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

  // Read in the data
  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuintfile(infp, &data);
  assert(data != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  initCrossRCT(configRCTC_literal, configRCTC_cross, &healthTests);

  // If configVerbose > 1, then we'll keep track of the transitions.
  // This functionality is mainly useful to establish appropriate cutoff settings,
  // and would not generally be present in a deployed entropy source.
  if (configSuggestCutoffs || (configVerbose > 1)) {
    for (unsigned int j = 0; j < CROSS_RCT_TESTS; j++) {
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
    for(size_t j=0; j<CROSS_RCT_TESTS; j++) maxRunCountLength = (maxRunCountLength<healthTests.runCountLengths[j])?healthTests.runCountLengths[j]:maxRunCountLength;
    consolidatedLiteralRunCounts = calloc(maxRunCountLength, sizeof(size_t));
    assert(consolidatedLiteralRunCounts != NULL);
    consolidatedCrosswiseRunCounts = calloc(maxRunCountLength, sizeof(size_t));
    assert(consolidatedCrosswiseRunCounts != NULL);
  }

  // Not all of the results are actually distinct (because XOR is commutative)
  // This array keeps track of which values have been reported.
  for (unsigned int j = 0; j < MAX_CROSSWISE_PAIR_LABEL; j++) outputPair[j] = MAX_CROSSWISE_PAIR_LABEL;

  // Convention: shiftByteIndex 0 is the index for the literal RO values,
  // which we'll call "data".
  // If we denote "circular shift right" for 8-bit values as rotl8,
  // then shiftByteIndex j (j>0) is the index for data ^ rotl8(data, j).
  for (uint8_t shiftByteIndex = 0; shiftByteIndex < CROSS_RCT_SHIFT_BYTES; shiftByteIndex++) {
    // bitIndex is the bit position of the selected byte.
    for (uint8_t bitIndex = 0; bitIndex < 8; bitIndex++) {
      uint8_t bitloc = (uint8_t)((shiftByteIndex << 3) | bitIndex);  // The bit position of the identified shiftByteIndex and bitIndex.
      uint8_t canonicalPairLabel = 0;
      char label[20];
      bool reportLocation;


      if (shiftByteIndex == 0) {
        // Always report on the literal RO copy RCTs.
        sprintf(label, "Literal osc %u", bitIndex);
        reportLocation = true;
      } else if(bitloc < CROSS_RCT_TESTS) {
        uint8_t maxIndex, minIndex;
        // This is one of the crosswise RCT values
        // Here, we identify the contents of the targeted bit.
        // It is bit `bitIndex` in the byte rotl8(data, j)
        // Call the bit index of bit k of rotl8(data, j) (where the lsb is bit 0) bindex(j,k)
        // Note that we can calculate all the bit indices using the bit index of bit 0 (here, bindex(j,0)), as
        // bindex(j,k) = ( bindex(j,0) + k ) mod 8.
        // We note that: bindex(0,0) is 0
        // bindex(1,0) is 7
        // bindex(2,0) is 6
        //...
        // bindex(j,0) is (8-j) mod 8
        // Note, the use of "8-j" here is best understood as "-j mod 8".
        uint8_t xorLoc = ((8 - shiftByteIndex) + bitIndex) % 8;

        // We don't ever XOR a bit location with itself.
        assert(xorLoc != bitIndex);

        // Note that both xorLoc and bitIndex are in the range [0,7], so they are encoded in 3 bits.
        //
        // We create 6-bit canonical labels by forcing the smaller xorLoc and bitIndex to be the more significant 3 bits,
        // and the larger of the two to be the least significant 3 bits.
        //
        // The canonical labels are useful, because
        // XOR is commutative, so the results for the tests for labels
        // "((xorLoc << 3) | bitIndex)" and "((bitIndex << 3) | xorLoc)" are the same.

        // Build up the canonical label for this pair
        if (xorLoc < bitIndex) {
          minIndex = xorLoc;
          maxIndex = bitIndex;
        } else {
          minIndex = bitIndex;
          maxIndex = xorLoc;
        }

        canonicalPairLabel = (uint8_t)((minIndex << 3) | maxIndex);

        // If we haven't output a value yet, do so.
        if (outputPair[canonicalPairLabel] == MAX_CROSSWISE_PAIR_LABEL) {
          // Save the bit location that we use for this canonical label
          outputPair[canonicalPairLabel] = bitloc;

          // This is new, so we report on this RCT.
          reportLocation = true;
          sprintf(label, "XOR of osc (%u, %u)", minIndex, maxIndex);
        } else {
          // We've already reported on this canonical label.
          reportLocation = false;
        }
      } else {
        reportLocation = false;
      }

      if (reportLocation) {
        // Remember which bytes we use.
        if (maxShiftIndex < shiftByteIndex) maxShiftIndex = shiftByteIndex;

        if((healthTests.RCT_C[bitloc] > 0) || (configVerbose > 1)) printf("RCT %s: ", label);

        assert(healthTests.RCT_Input >= healthTests.RCT_C[bitloc]);
        RCT_Count = healthTests.RCT_Input - healthTests.RCT_C[bitloc] + 1;

        if(healthTests.RCT_C[bitloc] > 0) {
          // We were asked to report on this RCT and the failure rates might mean something.
          printf("Failure Rate: %zu / %zu = %g", healthTests.RCT_Failures[bitloc], RCT_Count, ((double)(healthTests.RCT_Failures[bitloc])) / ((double)RCT_Count));
          if (healthTests.RCT_Failures[bitloc] > 0) {
            printf(" ≈ 2^%g\n", log2(((double)(healthTests.RCT_Failures[bitloc])) / ((double)RCT_Count)));
          } else {
            printf("\n");
          }
        }

        // If we've been keeping count of the run lengths, print it out.
        if (healthTests.runCounts[bitloc] != NULL) {
          char startChar = '{';
          for (size_t i = 0; i < healthTests.runCountLengths[bitloc]; i++) {
            // Save the length of the largest run.
            if ((healthTests.runCounts[bitloc])[i] > 0) {
              if (i > maxRunLength) maxRunLength = i;

              if (configSuggestCutoffs) {
                if (shiftByteIndex == 0)
                  consolidatedLiteralRunCounts[i] += (healthTests.runCounts[bitloc])[i];
                else
                  consolidatedCrosswiseRunCounts[i] += (healthTests.runCounts[bitloc])[i];
              }
            }

            if ((configVerbose > 1) && ((healthTests.runCounts[bitloc])[i] != 0)) {
              printf("%c {%zu, %zu}", startChar, i, healthTests.runCounts[bitloc][i]);
              startChar = ',';
            }
          }
          if (configVerbose > 1) printf("};\n");
        }
      } else if(bitloc < CROSS_RCT_TESTS) {
        assert(canonicalPairLabel != 0);
        // We were not asked to report on this RCT.
        // We can still verify that the results match between this RCT, and the relevant canonical label RCT.
        assert(healthTests.RCT_Failures[bitloc] == healthTests.RCT_Failures[outputPair[canonicalPairLabel]]);
        if (healthTests.runCounts[bitloc] != NULL) {
          assert(healthTests.runCountLengths[bitloc] == healthTests.runCountLengths[outputPair[canonicalPairLabel]]);
          for (size_t i = 0; i < healthTests.runCountLengths[bitloc]; i++) {
            assert(healthTests.runCounts[bitloc][i] == healthTests.runCounts[outputPair[canonicalPairLabel]][i]);
          }
        }
      }
    }
  }

  if(maxRunLength > 0) printf("Longest encountered run: %zu\n", maxRunLength);
  if(configRCTC_literal > 0) {
    if(healthTests.maxLiteralFailures > 0) printf("Most simultaneous literal failures: %u\n", healthTests.maxLiteralFailures);
    else printf("No recorded literal failures.\n");
  }

  if(configRCTC_cross > 0) {
    if(healthTests.maxCrossFailures > 0) printf("Most simultaneous cross failures: %u\n", healthTests.maxCrossFailures);
    else printf("No recorded cross failures.\n");
  }

  fflush(stdout);

  if (configSuggestCutoffs) {
    // Just add to either the crosswise or literal bucket list.
    long double alpha = powl(2.0L, -(long double)configAlphaExp);
    // There are a total of 36 distinct tests that we are treating as independent.
    long double perTestAlpha = 1.0L - powl(1.0L - alpha, 1.0L / ((long double)36.0L));
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

  // Check to see if everything was tested.
  if (configVerbose > 0) {
    bool completeTestCoverage = true;

    for (uint8_t j = 0; j < MAX_CROSSWISE_PAIR_LABEL; j++) {
      uint8_t curCanonicalLabel;
      uint8_t minIndex, maxIndex;

      curCanonicalLabel = bitIndexToCanonicalLabel(j);
      minIndex = (uint8_t)(curCanonicalLabel >> 3);
      maxIndex = curCanonicalLabel & 0x07;
      if ((minIndex != maxIndex) && (outputPair[curCanonicalLabel] == MAX_CROSSWISE_PAIR_LABEL)) {
        completeTestCoverage = false;
        fprintf(stderr, "Missing pair (%" PRIu8 ", %" PRIu8 ") via bitIndex %u\n", (uint8_t)(curCanonicalLabel >> 3), (uint8_t)(curCanonicalLabel & 0x07), j);
      }
    }

    if (completeTestCoverage) {
      if (CROSS_RCT_SHIFT_BYTES != maxShiftIndex + 1) fprintf(stderr, "CROSS_RCT_SHIFT_BYTES should be set to %" PRIu8 "\n", (uint8_t)(maxShiftIndex + 1));
    } else {
      fprintf(stderr, "CROSS_RCT_SHIFT_BYTES should be increased.\n");
    }
  }

  // Free all the allocated memory.
  for (unsigned int j = 0; j < CROSS_RCT_TESTS; j++) {
    if (healthTests.runCounts[j] != NULL) {
      free(healthTests.runCounts[j]);
      healthTests.runCounts[j] = NULL;
    }
  }

  if (configSuggestCutoffs) {
    free(consolidatedLiteralRunCounts);
    consolidatedLiteralRunCounts = NULL;
    free(consolidatedCrosswiseRunCounts);
    consolidatedCrosswiseRunCounts = NULL;
  }

  free(data);
  return EX_OK;
}
