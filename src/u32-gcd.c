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
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-gcd <filename>\n");
  fprintf(stderr, "Find common divisors, and remove these factors.\n");
  fprintf(stderr, "The values are output to stdout.\n");
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

static uint32_t gcd(uint32_t a, uint32_t b) {
  // Make a greater than or equal.
  if (a < b) {
    uint32_t c = a;
    a = b;
    b = c;
  }

  while (b != 0) {
    uint32_t r;

    r = a % b;

    a = b;
    b = r;
  }

  return a;
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  uint32_t *gcds = NULL;
  size_t i;
  uint32_t curfactor;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  fprintf(stderr, "Read in %" PRId64 " uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  if ((gcds = malloc(sizeof(uint32_t) * datalen)) == NULL) {
    perror("Can't allocate gcd array");
    exit(EX_OSERR);
  }

  if (datalen < 2) {
    fprintf(stderr, "Too little data\n");
    exit(EX_DATAERR);
  }

  curfactor = data[0];
  for (i = 1; i < datalen; i++) {
    gcds[i] = gcd(data[i - 1], data[i]);
    if (curfactor != 1) curfactor = gcd(data[i], curfactor);
  }

  if (curfactor > 1) {
    fprintf(stderr, "Found common divisor %u\n", curfactor);
    for (i = 0; i < datalen; i++) {
      data[i] /= curfactor;
    }
  } else {
    //Look for an almost common factor
    uint32_t mostCommonGCD = 0;
    uint32_t curGCD = 0;
    size_t curcount = 0;
    size_t maxcount = 0;
    double mostCommonGCDrate;

    qsort(gcds, datalen - 1, sizeof(uint32_t), uintcompare);
    for (i = 0; i < datalen - 1; i++) {
      if (gcds[i] == curGCD) {
        curcount++;
      } else {
        curcount = 1;
        curGCD = gcds[i];
      }

      if (curcount >= maxcount) {
        mostCommonGCD = curGCD;
        maxcount = curcount;
      }
    }

    mostCommonGCDrate = ((double)maxcount) / ((double)(datalen - 1));

    fprintf(stderr, "Most common gcd is %u (%g)\n", mostCommonGCD, mostCommonGCDrate);
    if ((mostCommonGCDrate > 0.99) && (mostCommonGCD > 1)) {
      fprintf(stderr, "Ignoring fuzz\n");
      for (i = 0; i < datalen; i++) {
        data[i] /= mostCommonGCD;
      }
    }
  }

  if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
    if (data != NULL) {
      free(data);
      data = NULL;
    }
    if (gcds != NULL) {
      free(gcds);
      gcds = NULL;
    }

    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  if (data != NULL) {
    free(data);
    data = NULL;
  }
  if (gcds != NULL) {
    free(gcds);
    gcds = NULL;
  }

  return (0);
}
