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
#include <float.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include <getopt.h>  // for getopt, optarg, optind
#include <stdint.h>  // for uint32_t

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "restart-transpose [-v] [-l <index> ] [-d <samples>,<restarts>] <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to be a sequence of " STATDATA_STRING " integers\n");
  fprintf(stderr, "output is sent to stdout\n");
  fprintf(stderr, "-v \t verbose mode.\n");
  fprintf(stderr, "-l <index>\tRead the <index> substring of length <samples * restarts>. (default index = 0)\n");
  fprintf(stderr, "-d <samples>,<restarts>\tPerform the restart testing using the described matrix dimensions. (default is 1000x1000)\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  int opt;
  statData_t *data = NULL;
  size_t datalen;
  unsigned long long int inint;
  char *nextOption;
  size_t i, j;

  size_t configSubsetIndex;
  size_t configRestarts;
  size_t configSamplesPerRestart;

  configSubsetIndex = 0;
  configRestarts = 1000;
  configSamplesPerRestart = 1000;

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  while ((opt = getopt(argc, argv, "vl:d:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'd':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          useageExit();
        }
        configRestarts = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSamplesPerRestart = inint;
        break;
      case 'l':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSubsetIndex = inint;
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

  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  assert(configRestarts * configSamplesPerRestart > 0);

  datalen = readuintfileloc(infp, &data, configSubsetIndex, configRestarts * configSamplesPerRestart);
  assert(data != NULL);
  assert(datalen == configRestarts * configSamplesPerRestart);

  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Transpose %zu samples.\n", datalen);
  }

  for (i = 0; i < configSamplesPerRestart; i++) {
    for (j = 0; j < configRestarts; j++) {
      if (fwrite(data + configSamplesPerRestart * j + i, sizeof(statData_t), 1, stdout) != 1) {
        perror("Can't write output to stdout");
        exit(EX_OSERR);
      }
    }
  }

  free(data);

  return EX_OK;
}
