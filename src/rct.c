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
#include "health-tests.h"

/*
 * This program is a demonstration of the RCT health test.
 * This program is both a proof-of-concept for this health test,
 * and a tool that can be used to help establish reasonable cutoffs
 * for the RCT.
 */

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "rct [-v] [-r <value>] [-t <value>] [-d <value>] <infile>\n");
  fprintf(stderr, "Runs the SP 800-90B RCT health test on provided values.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-c <value>\tThe RCT cutoff value is <value>.\n");
  fprintf(stderr, "-t <value>\tTry to find suggested cutoff values, if the desired (per-symbol) false positive rate is 2^-<value>.\n");
  fprintf(stderr, "-d <value>\tData is presumed to be <value>-bit wide symbols. (supported values are 8, 32, and 64-bits).\n");
  fprintf(stderr, "-b <value>\tData is bitwise ANDed with <value>.\n");
  exit(EX_USAGE);
}

#define INITIAL_RUN_LIMIT 128

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *u8Data = NULL;
  uint32_t *u32Data = NULL;
  uint64_t *u64Data = NULL;
  size_t configRCTC;
  int opt;
  unsigned long long inint;
  double indouble;
  struct RCTstate healthTest;
  size_t RCT_Count;
  bool configSuggestCutoffs = false;
  size_t maxRunLength = 0;
  double configAlphaExp = 0;
  uint32_t configBitWidth = 0;
  uint64_t configAND = 0xffffffffffffffffULL;
  char *endptr=NULL;

  configVerbose = 0;
  configRCTC = 0;

  while ((opt = getopt(argc, argv, "vc:t:d:b:")) != -1) {
    switch (opt) {
      case 'v':
        // Output more debug information.
        configVerbose++;
        break;
      case 'b':
        // AND with the provided 64-bit value
        endptr=NULL;
        inint = strtoull(optarg, &endptr, 0);
        if (((inint == ULLONG_MAX) && (errno == ERANGE)) || ((inint == 0) && (endptr == optarg))) {
          fprintf(stderr, "Can't interpret bitmask value\n");
          useageExit();
        }
        configAND = (size_t)inint;
        break;
      case 'c':
        // Set the RCT bound.
        endptr=NULL;
        inint = strtoull(optarg, &endptr, 0);
        if (((inint == 0) && (endptr == optarg)) || ((inint == ULLONG_MAX) && (errno == EINVAL)) || (inint > SIZE_MAX)) {
          fprintf(stderr, "Can't interpret cutoff value\n");
          useageExit();
        }
        configRCTC = (size_t)inint;
        break;
      case 'd':
        endptr=NULL;
        inint = strtoull(optarg, &endptr, 0);
        if (((inint == 0) && (endptr == optarg)) || ((inint == ULLONG_MAX) && (errno == ERANGE)) || ((inint != 8) && (inint != 32) &&(inint != 64))) {
          fprintf(stderr, "Unsupported symbol size\n");
          useageExit();
        }
        configBitWidth = (uint32_t)inint;
        break;
      case 't':
        // Estimate the appropriate cutoffs.
        configSuggestCutoffs = true;

        endptr=NULL;
        indouble = strtod(optarg, &endptr);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        //These double values are flags set by "strtod", so exact comparison is correct.
        if (((indouble == 0.0) && (endptr == optarg)) || (((indouble == HUGE_VAL) || (indouble == -HUGE_VAL)) && errno == ERANGE)) {
          fprintf(stderr, "Can't interpret target alphaExp value\n");
          useageExit();
        }
#pragma GCC diagnostic pop
        configAlphaExp = indouble;
        if ((configAlphaExp < 20.0) || (configAlphaExp > 40.0)) fprintf(stderr, "Desired alphaExp of %g is outside of the recommended interval [20, 40].\n", configAlphaExp);
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
  } else if(configBitWidth == 32) {
    datalen = readuint32file(infp, &u32Data);
    assert(u32Data != NULL);
  } else if(configBitWidth == 64) {
    datalen = readuint64file(infp, &u64Data);
    assert(u64Data != NULL);
  } else {
    useageExit();
  }

  assert(datalen > 0);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }

  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  if((configVerbose > 0) && (configRCTC > 0)) {
    printf("RCT cutoff: %zu\n", configRCTC);
  }


  initRCT(configRCTC, &healthTest);

  // If configVerbose > 1, then we'll keep track of the transitions.
  // This functionality is mainly useful to establish appropriate cutoff settings,
  // and would not generally be present in a deployed entropy source.
  if (configSuggestCutoffs || (configVerbose > 1)) {
    healthTest.runCounts = calloc(INITIAL_RUN_LIMIT, sizeof(size_t));
    assert(healthTest.runCounts != NULL);
    healthTest.runCountsLength = INITIAL_RUN_LIMIT;
  }

  // Feed in all the data to the RCT test.
  for (size_t i = 0; i < datalen; i++) {
    uint64_t curData;
    if(configBitWidth==8) curData = (uint64_t)u8Data[i];
    else if(configBitWidth==32) curData = (uint64_t)u32Data[i];
    else curData = u64Data[i];

    RCT((curData & configAND), &healthTest);
  }

  // Report on the results of the test.
  if (healthTest.RCT_C > 0) {
    // We were asked to report on this RCT and the failure rates might mean something.
    assert(healthTest.RCT_Input >= healthTest.RCT_C);
    RCT_Count = healthTest.RCT_Input - healthTest.RCT_C + 1;
    printf("RCT: Failure Rate: %zu / %zu = %g", healthTest.RCT_Failures, RCT_Count, ((double)(healthTest.RCT_Failures)) / ((double)RCT_Count));
    if (healthTest.RCT_Failures > 0) {
      printf(" ≈ 2^%g\n", log2(((double)(healthTest.RCT_Failures)) / ((double)RCT_Count)));
    } else {
      printf("\n");
    }
  }

  // If we've been keeping count of the run lengths, print it out.
  if (healthTest.runCounts != NULL) {
    char startChar = '{';
    for (size_t i = 0; i < healthTest.runCountsLength; i++) {
      // Save the length of the largest run.
      if (healthTest.runCounts[i] > 0) maxRunLength = i;

      if ((configVerbose > 1) && ((healthTest.runCounts)[i] != 0)) {
        printf("%c {%zu, %zu}", startChar, i, healthTest.runCounts[i]);
        startChar = ',';
      }
    }
    if (configVerbose > 1) printf("};\n");
  }

  if(maxRunLength > 0) printf("Longest encountered run: %zu\n", maxRunLength);

  if (configSuggestCutoffs) {
    long double alpha = powl(2.0L, -(long double)configAlphaExp);
    size_t allowedFailures = (size_t)floorl(((long double)datalen) * alpha);
    size_t accumulatedFailures;

    if (configVerbose > 0) fprintf(stderr, "Allowed failures: %zu\n", allowedFailures);

    if (allowedFailures < 10) fprintf(stderr, "The bound for the desired number of failures (%zu < 10) may not yield statistically meaningful results.\n", allowedFailures);

    accumulatedFailures = 0;
    for (size_t j = maxRunLength; j > 0; j--) {
      accumulatedFailures += healthTest.runCounts[j];
      if (accumulatedFailures > allowedFailures) {
        printf("Empirical RCT cutoff: %zu\n", j + 1);
        break;
      }
    }
  }

  // Free all the allocated memory.
  if (healthTest.runCounts != NULL) {
    free(healthTest.runCounts);
    healthTest.runCounts = NULL;
  }

  if(u64Data!=NULL) free(u64Data);
  if(u32Data!=NULL) free(u32Data);
  if(u8Data!=NULL) free(u8Data);

  return EX_OK;
}
