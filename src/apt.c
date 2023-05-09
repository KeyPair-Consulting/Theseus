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
#include "health-tests.h"

/*
 * This program is a demonstration of the APT health test.
 * This program is both a proof-of-concept for this health test,
 * and a tool that can be used to help establish reasonable cutoffs
 * for the APT.
 */

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "apt [-v] [-c <value>] [-w <value>] [-t <value>] [-d <value>] <infile>\n");
  fprintf(stderr, "Runs the SP 800-90B APT health test on provided values.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-c <value>\tThe APT cutoff value is <value>.\n");
  fprintf(stderr, "-w <value>\tThe APT window value is <value>. (Default is 512 symbols.)\n");
  fprintf(stderr, "-t <value>\tTry to find suggested cutoff values, if the desired (per-window) false positive rate is 2^-<value>.\n");
  fprintf(stderr, "-d <value>\tData is presumed to be <value>-bit wide symbols. (supported values are 8, 32, and 64-bits).\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  statData_t *u8Data = NULL;
  uint32_t *u32Data = NULL;
  uint64_t *data = NULL;
  size_t configAPTC;
  size_t configAPTW;
  int opt;
  unsigned long long inint;
  struct APTstate healthTest;
  bool configSuggestCutoffs = false;
  uint32_t configAlphaExp = 0;
  size_t maxRunLength = 0;
  uint32_t configBitWidth = 0;

  configVerbose = 0;
  configAPTC = 0;
  configAPTW = 512;

  while ((opt = getopt(argc, argv, "vc:w:t:d:")) != -1) {
    switch (opt) {
      case 'v':
        // Output more debug information.
        configVerbose++;
        break;
      case 'c':
        // Set the APT bound.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAPTC = (size_t)inint;
        break;
      case 'w':
        // Set the APT window.
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAPTW = (size_t)inint;
        break;
      case 'd':
        inint = strtoull(optarg, NULL, 0);
        if ((errno == EINVAL) || ((inint != 8) && (inint != 32) &&(inint != 64))) {
          useageExit();
        }
        configBitWidth = (uint32_t)inint;
        if ((configAlphaExp < 20) || (configAlphaExp > 40)) fprintf(stderr, "Desired alphaExp of %u is outside of the recommended interval [20, 40].\n", configAlphaExp);
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
    free(u8Data);
  } else if(configBitWidth == 32) {
    datalen = readuint32file(infp, &u32Data);
    assert(u32Data != NULL);
    assert(datalen > 0);
    data = calloc(datalen, sizeof(uint64_t));
    assert(data!=NULL);
    for(size_t j=0; j<datalen; j++) data[j] = u32Data[j];
    free(u32Data);
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


  initAPT(configAPTC, configAPTW, &healthTest);

  // If configVerbose > 1, then we'll keep track of the window statistics.
  // This functionality is mainly useful to establish appropriate cutoff settings,
  // and would not generally be present in a deployed entropy source.
  if (configSuggestCutoffs || (configVerbose > 1)) {
    healthTest.APTcounts = calloc(configAPTW+1, sizeof(size_t));
    assert(healthTest.APTcounts != NULL);
  }

  // Feed in all the data to the APT test.
  for (size_t i = 0; i < datalen; i++) {
    APT(data[i], &healthTest);
  }

  // Report on the results of the test.
  if (healthTest.APT_C > 0) {
    // We were asked to report on this APT and the failure rates might mean something.
    assert(healthTest.APT_Window_Count >= 1);
    printf("APT: Per Window Failure Rate: %zu / %zu = %g", healthTest.APT_Failures, healthTest.APT_Window_Count, ((double)(healthTest.APT_Failures)) / ((double)healthTest.APT_Window_Count));
    if (healthTest.APT_Failures > 0) {
      printf(" ≈ 2^%g\n", log2(((double)(healthTest.APT_Failures)) / ((double)healthTest.APT_Window_Count)));
    } else {
      printf("\n");
    }
  }

  // If we've been keeping count of the run lengths, print it out.
  if (healthTest.APTcounts != NULL) {
    char startChar = '{';
    for (size_t i = 0; i <= configAPTW; i++) {
      if ((configVerbose > 1) && ((healthTest.APTcounts)[i] != 0)) {
        printf("%c {%zu, %zu}", startChar, i, healthTest.APTcounts[i]);
        startChar = ',';
      }
    }
    if (configVerbose > 1) printf("};\n");
  }

  if(maxRunLength > 0) printf("Longest encountered run: %zu\n", maxRunLength);

  if (configSuggestCutoffs) {
    long double alpha = powl(2.0L, -(long double)configAlphaExp);
    size_t allowedFailures = (size_t)floorl(((long double)healthTest.APT_Window_Count) * alpha);
    size_t accumulatedFailures;

    assert(healthTest.APTcounts != NULL);

    if (configVerbose > 0) fprintf(stderr, "Allowed failures: %zu\n", allowedFailures);

    if (allowedFailures < 10) fprintf(stderr, "The bound for the desired number of failures (%zu < 10) may not yield statistically meaningful results.\n", allowedFailures);

    accumulatedFailures = 0;

    for (size_t j = configAPTW; j > 0; j--) {
      accumulatedFailures += healthTest.APTcounts[j];
      if (accumulatedFailures > allowedFailures) {
        printf("Empirical APT cutoff: %zu\n", j + 1);
        break;
      }
    }
  }

  // Free all the allocated memory.
  if (healthTest.APTcounts != NULL) {
    free(healthTest.APTcounts);
    healthTest.APTcounts = NULL;
  }

  free(data);
  return EX_OK;
}
