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
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <math.h>

#include "precision.h"
#include "fancymath.h"

int main(void) {
  size_t res;
  size_t symbolCount[UINT16_MAX+1] = {0};
  size_t maxSymbolCount = 0;
  size_t symbols = 0;
  uint16_t maxSymbol = 0;
  double pu;
  double phat;


  while (feof(stdin) == 0) {
    uint16_t indata;

    res = fread(&indata, sizeof(uint16_t), 1, stdin);
    if (res == 1) {
      symbolCount[indata]++;
      symbols++;
    }
  }

  for(uint32_t j=0; j<=UINT16_MAX; j++) {
    if(symbolCount[j] > maxSymbolCount) {
      maxSymbolCount = symbolCount[j];
      maxSymbol = (uint16_t)j;
    }
  }
  fprintf(stderr, "Most common symbol is 0x%4X (count %zu)\n", maxSymbol, maxSymbolCount);

  phat=((double)maxSymbolCount) / ((double)symbols);
  // Note, this is the raw value. A higher value maps to a more conservative estimate, so we then look at upper confidence interval bound.

  // Note, this is a local guess at a confidence interval, under the assumption that this most probable symbol proportion is distributed
  // as per the binomial distribution (this CI estimate is made under a normal approximation of the binomial distribution);
  // this assumption isn't reasonable, as the most probable symbol can never have less than ceil(L/k) symbols,
  // so the entire low end of the binomial distribution isn't in the support for the actual distribution.
  // If we (a priori) knew what the MLS was, we could make this estimate, but we don't here.
  // This actual distribution here is that of the maximum bin of a multinomial distribution, but this is complicated.
  pu = phat + ZALPHA * sqrt(phat * (1.0 - phat) / ((double)symbols - 1.0));
  if (pu > 1.0) {
    pu = 1.0;
  }
  fprintf(stderr, "p_hat = %.17g\n", phat);
  fprintf(stderr, "p_u = %.17g\n", pu);
  fprintf(stderr, "minentropy = %.17g\n", -log2(pu));

  return 0;
}
