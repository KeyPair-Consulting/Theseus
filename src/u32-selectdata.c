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
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-selectdata inputfile trimLowPercent trimHighPercent\n");
  fprintf(stderr, "Attempt to keep the percentages noted.\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output is to stdout, and is u32 ints\n");
  exit(EX_USAGE);
}

static int uintcompare(const void *in1, const void *in2) {
  const uint32_t *left;
  const uint32_t *right;

  left = in1;
  right = in2;

  if (*left < *right) {
    return (-1);
  } else if (*left > *right) {
    return (1);
  } else {
    return (0);
  }
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t *data = NULL;
  uint32_t *sorteddata = NULL;
  size_t i;
  size_t datalen;
  uint32_t lowValue, highValue;
  size_t highIndex, lowIndex;
  double trimLowPercent;
  double trimHighPercent;
  double tmpIndex;

  if (argc != 4) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  trimLowPercent = atof(argv[2]);
  if ((trimLowPercent < 0.0) || (trimLowPercent >= 1.0)) {
    useageExit();
  }

  trimHighPercent = atof(argv[3]);
  if ((trimHighPercent < 0.0) || (trimHighPercent >= 1.0)) {
    useageExit();
  }

  datalen = readuint32file(infp, &data);
  if (datalen < 2) {
    useageExit();
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu samples\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  if ((trimHighPercent > 0.0) || (trimLowPercent > 0.0)) {
    if ((sorteddata = malloc(datalen * sizeof(uint32_t))) == NULL) {
      perror("Can't allocate array for sorted data table");
      exit(EX_OSERR);
    }

    fprintf(stderr, "Copying data\n");
    memcpy(sorteddata, data, datalen * sizeof(uint32_t));

    fprintf(stderr, "Sorting data\n");
    qsort(sorteddata, datalen, sizeof(uint32_t), uintcompare);

    if (trimHighPercent > 0.0) {
      tmpIndex = ceil(((double)(datalen)) - ((double)(datalen)) * trimHighPercent) - 1.0;
      if (tmpIndex > 1.0) {
        highIndex = (size_t)tmpIndex - 1;
      } else {
        highIndex = 0;
      }

      if (highIndex >= datalen - 1) {
        highIndex = datalen - 1;
      }
    } else {
      highIndex = datalen - 1;
    }

    if (trimLowPercent > 0.0) {
      tmpIndex = floor(((double)datalen) * trimLowPercent) - 1.0;
      if (tmpIndex > 1.0) {
        lowIndex = (size_t)tmpIndex - 1;
      } else {
        lowIndex = 0;
      }

      if (lowIndex >= datalen - 1) {
        lowIndex = datalen - 1;
      }
    } else {
      lowIndex = 0;
    }

    fprintf(stderr, "Getting the %zu to the %zu entries\n", lowIndex, highIndex);
    lowValue = sorteddata[lowIndex];
    highValue = sorteddata[highIndex];

    assert(sorteddata != NULL);
    free(sorteddata);
    sorteddata = NULL;
  } else {
    lowValue = 0;
    highValue = 0xFFFFFFFF;
  }

  fprintf(stderr, "MinValue = %u\n", lowValue);
  fprintf(stderr, "MaxValue = %u\n", highValue);

  fprintf(stderr, "Outputting the data...\n");
  for (i = 0; i < datalen; i++) {
    if ((data[i] >= lowValue) && (data[i] <= highValue)) {
      if (fwrite(&(data[i]), sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write output to stdout");
        exit(EX_OSERR);
      }
    }
  }

  free(data);
  return EX_OK;
}
