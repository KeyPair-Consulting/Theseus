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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <math.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "fancymath.h"

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

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-mcv <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  size_t maxSymbolCount = 0;
  uint32_t MLS = 0;
  size_t curSymbolCount = 0;
  size_t k = 0;
  uint32_t curSymbol;
  double pu;
  double phat;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if ((datalen = readuint32file(infp, &data)) < 1) {
    perror("Data file is empty");
    exit(EX_DATAERR);
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  assert(datalen > 0);

  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  //Sort the data
  qsort(data, datalen, sizeof(uint32_t), uintcompare);

  MLS = curSymbol = data[0];
  curSymbolCount = 1;

  for(size_t j=1; j<datalen; j++) {
    if(data[j] == curSymbol) {
       curSymbolCount ++;
    } else {
       //This is a transition.
       k++;
       if(curSymbolCount > maxSymbolCount) {
          maxSymbolCount = curSymbolCount;
          MLS = curSymbol;
       }

       curSymbolCount = 1;
       curSymbol = data[j];
    }
  }
  //This is a transition.
  k++;
  if(curSymbolCount > maxSymbolCount) {
    maxSymbolCount = curSymbolCount;
    MLS = curSymbol;
  }

  fprintf(stderr, "Encountered %zu distinct symbols.\n", k);
  fprintf(stderr, "Most common symbol is 0x%08X (count %zu)\n", MLS, maxSymbolCount);

  phat=((double)maxSymbolCount) / ((double)datalen);
  // Note, this is the raw value. A larger value maps to a more conservative estimate, so we then look at upper confidence interval bound.

  // Note, this is a local guess at a confidence interval, under the assumption that this most probable symbol proportion is distributed
  // as per the binomial distribution (this CI estimate is made under a normal approximation of the binomial distribution);
  // this assumption isn't reasonable, as the most probable symbol can never have less than ceil(L/k) symbols,
  // so the entire low end of the binomial distribution isn't in the support for the actual distribution.
  // If we (a priori) knew what the MLS was, we could make this estimate, but we don't here.
  // This actual distribution here is that of the maximum bin of a multinomial distribution, but this is complicated.
  pu = phat + ZALPHA * sqrt(phat * (1.0 - phat) / ((double)datalen - 1.0));
  if (pu > 1.0) {
    pu = 1.0;
  }
  fprintf(stderr, "p_hat = %.17g\n", phat);
  fprintf(stderr, "p_u = %.17g\n", pu);
  fprintf(stderr, "minentropy = %.17g\n", -log2(pu));
  free(data);

  return EX_OK;
}
