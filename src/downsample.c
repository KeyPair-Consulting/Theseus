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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "downsample [-b <block size>] <rate> <data file>\n");
  fprintf(stderr, "Groups data by index into modular classes mod <rate> evenly into the block size.\n");
  fprintf(stderr, "<rate>\tNumber of input samples per output samples\n");
  fprintf(stderr, "-b\tSamples per output block (default 1000000)\n");
  fprintf(stderr, "The " STATDATA_STRING " values are output via stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  size_t trimLen;
  uint32_t configRate;
  size_t configBlockSize;
  long int inparam;
  int opt;
  size_t datalen;
  statData_t *data;
  statData_t *outputBuffer;
  unsigned long long inint;
  char *nextOption;
  FILE *infp;
  size_t conjClass;
  size_t conjClassSubIndex;
  size_t conjClassPartitionSize;
  size_t j;

  configVerbose = 0;
  configBlockSize = 1000000;
  data = NULL;
  outputBuffer = NULL;

  while ((opt = getopt(argc, argv, "vb:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'b':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL)) {
          useageExit();
        }
        configBlockSize = inint;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 2) {
    useageExit();
  }

  inparam = strtol(argv[0], NULL, 10);
  if ((inparam <= 0) || (inparam > UINT32_MAX)) {
    useageExit();
  } else {
    configRate = (uint32_t)inparam;
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuintfile(infp, &data);
  assert(data != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  // Only deal with data than can be evenly partitioned into a multiple of configRate blocks, each of size configBlockSize
  // remove data

  trimLen = datalen % (configRate * configBlockSize);
  fprintf(stderr, "Trimming %zu samples\n", trimLen);
  datalen = datalen - trimLen;

  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);
  if ((outputBuffer = malloc(datalen * sizeof(statData_t))) == NULL) {
    perror("Can't allocate output buffer");
    exit(EX_OSERR);
  }

  conjClass = 0;
  conjClassSubIndex = 0;
  assert((datalen % configRate) == 0);
  conjClassPartitionSize = datalen / configRate;

  for (j = 0; j < datalen; j++) {
    outputBuffer[conjClass * conjClassPartitionSize + conjClassSubIndex] = data[j];
    conjClass++;
    if (conjClass == configRate) {
      conjClass = 0;
      conjClassSubIndex++;
    }
  }

  assert(conjClass == 0);
  assert((conjClassSubIndex % configBlockSize) == 0);

  free(data);

  if (fwrite(outputBuffer, sizeof(statData_t), datalen, stdout) != datalen) {
    perror("Can't write output to stdout");
    exit(EX_OSERR);
  }

  free(outputBuffer);

  return (0);
}
