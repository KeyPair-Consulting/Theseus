/* This file is part of the Theseus distribution.
 * Copyright 2020-2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "dictionaryTree.h"
#include "entlib.h"
#include "fancymath.h"
#include "globals.h"
#include "hashmodulus.h"
#include "precision.h"
#include "sa.h"

void initEntropyTestingResult(const char *label, struct entropyTestingResult *result) {
  assert(label != NULL);
  assert(result != NULL);

  strncpy(result->label, label, sizeof(result->label));
  result->label[sizeof(result->label) - 1] = '\0';

  result->mcv.done = false;
  result->cols.done = false;
  result->markov.done = false;
  result->comp.done = false;
  result->sa.done = false;
  result->sa.tTupleDone = false;
  result->sa.lrsDone = false;
  result->mcw.done = false;
  result->lag.done = false;
  result->mmc.done = false;
  result->lz78y.done = false;
  result->assessedEntropy = -1.0;
  result->assessedIIDEntropy = -1.0;
}

void printEntropyTestingResult(const struct entropyTestingResult *result) {
  /*6.3.1*/
  if (result->mcv.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s Most Common Value Estimate: Mode count = %zu\n", result->label, result->mcv.maxCount);
      fprintf(stderr, "%s Most Common Value Estimate: p-hat = %.17g\n", result->label, result->mcv.phat);
      fprintf(stderr, "%s Most Common Value Estimate: p_u = %.17g\n", result->label, result->mcv.pu);
    }
    fprintf(stderr, "%s Most Common Value Estimate: min entropy = %.17g\n", result->label, result->mcv.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->mcv.runTime);
  }

  /*6.3.2*/
  if (result->cols.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s Collision Estimate: v = %zu\n", result->label, result->cols.v);
      fprintf(stderr, "%s Collision Estimate: Sum t_i = %zu\n", result->label, result->cols.tSum);
      fprintf(stderr, "%s Collision Estimate: X-bar = %.17g\n", result->label, result->cols.mean);
      fprintf(stderr, "%s Collision Estimate: sigma-hat = %.17g\n", result->label, result->cols.stddev);
      fprintf(stderr, "%s Collision Estimate: X-bar' = %.17g\n", result->label, result->cols.meanbound);
      fprintf(stderr, "%s Collision Estimate: p = %.17g\n", result->label, result->cols.p);
    }

    fprintf(stderr, "%s Collision Estimate: min entropy = %.17g\n", result->label, result->cols.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->cols.runTime);
  }

  /*6.3.3*/
  if (result->markov.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s Markov Estimate: P_0 = %.17g\n", result->label, result->markov.P0);
      fprintf(stderr, "%s Markov Estimate: P_1 = %.17g\n", result->label, result->markov.P1);
      fprintf(stderr, "%s Markov Estimate: P_{0,0} = %.17g\n", result->label, result->markov.T[0][0]);
      fprintf(stderr, "%s Markov Estimate: P_{0,1} = %.17g\n", result->label, result->markov.T[0][1]);
      fprintf(stderr, "%s Markov Estimate: P_{1,0} = %.17g\n", result->label, result->markov.T[1][0]);
      fprintf(stderr, "%s Markov Estimate: P_{1,1} = %.17g\n", result->label, result->markov.T[1][1]);
      fprintf(stderr, "%s Markov Estimate: p-hat_max = %.17g\n", result->label, result->markov.phatmax);
    }
    fprintf(stderr, "%s Markov Estimate: min entropy = %.17g\n", result->label, result->markov.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->markov.runTime);
  }

  /*6.3.4*/
  if (result->comp.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s Compression Estimate: X-bar = %.17g\n", result->label, result->comp.mean);
      fprintf(stderr, "%s Compression Estimate: sigma-hat = %.17g\n", result->label, result->comp.stddev);
      fprintf(stderr, "%s Compression Estimate: X-bar' = %.17g\n", result->label, result->comp.meanbound);
      fprintf(stderr, "%s Compression Estimate: p = %.17g\n", result->label, result->comp.p);
    }

    fprintf(stderr, "%s Compression Estimate: min entropy = %.17g\n", result->label, result->comp.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->comp.runTime);
  }

  /*6.3.5 and 6.3.6*/
  if (result->sa.done) {
    if (configVerbose > 0) {
      if (result->sa.u > 1) {
        fprintf(stderr, "%s t-Tuple Estimate: t = %zu\n", result->label, result->sa.u - 1);
      } else {
        fprintf(stderr, "%s t-Tuple Estimate: t does not exist\n", result->label);
      }
      if (result->sa.tTuplePmax > 0.0) {
        fprintf(stderr, "%s t-Tuple Estimate: p-hat_max = %.17g\n", result->label, result->sa.tTuplePmax);
        fprintf(stderr, "%s t-Tuple Estimate: p_u = %.17g\n", result->label, result->sa.tTuplePu);
      } else {
        fprintf(stderr, "%s t-Tuple Estimate: No strings of suitable length. Unable to run.\n", result->label);
      }
    }

    if (result->sa.v < result->sa.u) {
      fprintf(stderr, "%s LRS Estimate: Can't run LRS test as v<u\n", result->label);
    } else {
      if (configVerbose > 0) {
        fprintf(stderr, "%s LRS Estimate: u = %zu\n", result->label, result->sa.u);
        fprintf(stderr, "%s LRS Estimate: v = %zu\n", result->label, result->sa.v);
        fprintf(stderr, "%s LRS Estimate: p-hat = %.17g\n", result->label, result->sa.lrsPmax);
        fprintf(stderr, "%s LRS Estimate: p_u = %.17g\n", result->label, result->sa.lrsPu);
      }
    }
  }

  if (result->sa.tTupleDone) {
    fprintf(stderr, "%s t-Tuple Estimate: min entropy = %.17g\n", result->label, result->sa.tTupleEntropy);
  }

  if (result->sa.lrsDone) {
    fprintf(stderr, "%s LRS Estimate: min entropy = %.17g\n", result->label, result->sa.lrsEntropy);
  }

  if ((result->sa.done) && (configVerbose > 1)) fprintf(stderr, "Test took %.17g s CPU time\n", result->sa.runTime);

  /*6.3.7*/
  if (result->mcw.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s MultiMCW Prediction Estimate: C = %zu\n", result->label, result->mcw.C);
      fprintf(stderr, "%s MultiMCW Prediction Estimate: r = %zu\n", result->label, result->mcw.r);
      fprintf(stderr, "%s MultiMCW Prediction Estimate: N = %zu\n", result->label, result->mcw.N);
      fprintf(stderr, "%s MultiMCW Prediction Estimate: P_global = %.17g\n", result->label, result->mcw.Pglobal);
      fprintf(stderr, "%s MultiMCW Prediction Estimate: P_global' = %.17g\n", result->label, result->mcw.PglobalBound);
      if (configVerbose > 2) fprintf(stderr, "%s MultiMCW Prediction Estimate: P_run = %.17g\n", result->label, result->mcw.Prun);
      if (result->mcw.Plocal >= 0.0) {
        fprintf(stderr, "%s MultiMCW Prediction Estimate: P_local = %.17g\n", result->label, result->mcw.Plocal);
      } else {
        fprintf(stderr, "%s MultiMCW Prediction Estimate: P_local can't change the result.\n", result->label);
      }
    }
    fprintf(stderr, "%s MultiMCW Prediction Estimate: min entropy = %.17g\n", result->label, result->mcw.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->mcw.runTime);
  }

  /*6.3.8*/
  if (result->lag.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s Lag Prediction Estimate: C = %zu\n", result->label, result->lag.C);
      fprintf(stderr, "%s Lag Prediction Estimate: r = %zu\n", result->label, result->lag.r);
      fprintf(stderr, "%s Lag Prediction Estimate: N = %zu\n", result->label, result->lag.N);
      fprintf(stderr, "%s Lag Prediction Estimate: P_global = %.17g\n", result->label, result->lag.Pglobal);
      fprintf(stderr, "%s Lag Prediction Estimate: P_global' = %.17g\n", result->label, result->lag.PglobalBound);
      if (configVerbose > 2) fprintf(stderr, "%s Lag Prediction Estimate: P_run = %.17g\n", result->label, result->lag.Prun);
      if (result->lag.Plocal >= 0.0) {
        fprintf(stderr, "%s Lag Prediction Estimate: P_local = %.17g\n", result->label, result->lag.Plocal);
      } else {
        fprintf(stderr, "%s Lag Prediction Estimate: P_local can't change the result.\n", result->label);
      }
    }
    fprintf(stderr, "%s Lag Prediction Estimate: min entropy = %.17g\n", result->label, result->lag.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->lag.runTime);
  }

  /*6.3.9*/
  if (result->mmc.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s MultiMMC Prediction Estimate: C = %zu\n", result->label, result->mmc.C);
      fprintf(stderr, "%s MultiMMC Prediction Estimate: r = %zu\n", result->label, result->mmc.r);
      fprintf(stderr, "%s MultiMMC Prediction Estimate: N = %zu\n", result->label, result->mmc.N);
      fprintf(stderr, "%s MultiMMC Prediction Estimate: P_global = %.17g\n", result->label, result->mmc.Pglobal);
      fprintf(stderr, "%s MultiMMC Prediction Estimate: P_global' = %.17g\n", result->label, result->mmc.PglobalBound);
      if (configVerbose > 2) fprintf(stderr, "%s MultiMMC Prediction Estimate: P_run = %.17g\n", result->label, result->mmc.Prun);
      if (result->mmc.Plocal >= 0.0) {
        fprintf(stderr, "%s MultiMMC Prediction Estimate: P_local = %.17g\n", result->label, result->mmc.Plocal);
      } else {
        fprintf(stderr, "%s MultiMMC Prediction Estimate: P_local can't change the result.\n", result->label);
      }
    }
    fprintf(stderr, "%s MultiMMC Prediction Estimate: min entropy = %.17g\n", result->label, result->mmc.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->mmc.runTime);
  }

  /*6.3.10*/
  if (result->lz78y.done) {
    if (configVerbose > 0) {
      fprintf(stderr, "%s LZ78Y Prediction Estimate: C = %zu\n", result->label, result->lz78y.C);
      fprintf(stderr, "%s LZ78Y Prediction Estimate: r = %zu\n", result->label, result->lz78y.r);
      fprintf(stderr, "%s LZ78Y Prediction Estimate: N = %zu\n", result->label, result->lz78y.N);
      fprintf(stderr, "%s LZ78Y Prediction Estimate: P_global = %.17g\n", result->label, result->lz78y.Pglobal);
      fprintf(stderr, "%s LZ78Y Prediction Estimate: P_global' = %.17g\n", result->label, result->lz78y.PglobalBound);
      if (configVerbose > 2) fprintf(stderr, "%s LZ78Y Prediction Estimate: P_run = %.17g\n", result->label, result->lz78y.Prun);
      if (result->lz78y.Plocal >= 0.0) {
        fprintf(stderr, "%s LZ78Y Prediction Estimate: P_local = %.17g\n", result->label, result->lz78y.Plocal);
      } else {
        fprintf(stderr, "%s LZ78Y Prediction Estimate: P_local can't change the result.\n", result->label);
      }
    }
    fprintf(stderr, "%s LZ78Y Prediction Estimate: min entropy = %.17g\n", result->label, result->lz78y.entropy);
    if (configVerbose > 1) fprintf(stderr, "Test took %.17g s CPU time\n", result->lz78y.runTime);
  }

  if (configVerbose > 1) {
    fprintf(stderr, "All tests took %.17g s CPU time\n", result->runTime);
  }
}

double shannonEntropyEstimate(const statData_t *S, size_t L, size_t k) {
  double entropy = 0.0;
  struct compensatedState entropyAccumulator;
#if STATDATA_BITS <= 8
  size_t count[256] = {0};
#else
  size_t *count;
#endif

  assert(L > 0);
  assert(k > 0);

  initCompensatedSum(&entropyAccumulator, "entropyAccumulator", 10);

#if STATDATA_BITS > 8
  if ((count = calloc(k, sizeof(size_t))) == NULL) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }
#else
  assert(k <= 256);
#endif

  // Count the symbols
  for (const statData_t *curdataptr = S; curdataptr < S + L; curdataptr++) {
    assert((size_t)*curdataptr < k);
    count[*curdataptr]++;
  }

  // We could have done this while summarizing the data, but that would result in
  // L comparisons, whereas this approach uses k comparisons, and k<<L.
  for (size_t i = 0; i < k; i++) {
    if (count[i] > 0) {
      double p = (double)count[i] / (double)L;
      if (configVerbose > 2) fprintf(stderr, "p[ X = %zu ] = %.17g\n", i, p);
      compensatedSum(&entropyAccumulator, p * log2(p));
    }
  }

  entropy = compensatedSumResult(&entropyAccumulator);
  delCompensatedSum(&entropyAccumulator);
#if STATDATA_BITS > 8
  free(count);
#endif

  return (-entropy);
}

/*SP800-90B-final 6.3.1*/
double mostCommonValueEstimate(const statData_t *S, size_t L, size_t k, struct MCVresult *result) {
  size_t maxCount;
#if STATDATA_BITS <= 8
  size_t count[256] = {0};
#else
  size_t *count;
#endif

  assert(L > 0);
  assert(k > 0);
  assert(S != NULL);
  assert(result != NULL);

#if STATDATA_BITS > 8
  if ((count = calloc(k, sizeof(size_t))) == NULL) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }
#else
  assert(k <= 256);
#endif

  // Count the symbols
  for (const statData_t *curdataptr = S; curdataptr < S + L; curdataptr++) {
    assert((size_t)*curdataptr < k);
    count[*curdataptr]++;
  }

  // We could have done this while summarizing the data, but that would result in
  // L comparisons, whereas this approach uses k comparisons, and k<<L.
  maxCount = 0;
  for (size_t i = 0; i < k; i++) {
    if (count[i] > maxCount) {
      maxCount = count[i];
    }
  }

  result->maxCount = maxCount;

  result->phat = ((double)(maxCount)) / (double)L;
  // Note, this is the raw value. A higher value maps to a more conservative estimate, so we then look at upper confidence interval bound.

  // Note, this is a local guess at a confidence interval, under the assumption that this most probable symbol proportion is distributed
  // as per the binomial distribution (this CI estimate is made under a normal approximation of the binomial distribution);
  // this assumption isn't reasonable, as the most probable symbol can never have less than ceil(L/k) symbols,
  // so the entire low end of the binomial distribution isn't in the support for the actual distribution.
  // If we (a priori) knew what the MLS was, we could make this estimate, but we don't here.
  // This actual distribution here is that of the maximum bin of a multinomial distribution, but this is complicated.
  result->pu = result->phat + ZALPHA * sqrt(result->phat * (1.0 - result->phat) / ((double)L - 1.0));
  if (result->pu > 1.0) {
    result->pu = 1.0;
  }

#if STATDATA_BITS > 8
  free(count);
#endif

  result->entropy = -log2(result->pu);
  result->done = true;

  return (result->entropy);
}

// SP800-90B-final 6.3.2
double collisionEstimate(const statData_t *S, size_t L, struct colsResult *result) {
  size_t i;
  int exceptions;

  assert(result != NULL);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  // We ultimately need to assure that v>=2
  assert(L >= 6);

  if (S != NULL) {
    size_t twoCount = 0;
    size_t threeCount = 0;
    size_t tSqSum;

    // There are only 2 symbols, so we only have two relevant cases.
    i = 0;
    while (i < L - 2) {
      if (S[i] == S[i + 1]) {
        // Either 00 or 11
        twoCount++;
        i += 2;
      } else {
        // The leading bits are 01 or 10, so this leaves us with the following cases:
        // 010, 011, 100, or 101
        threeCount++;
        i += 3;
      }
    }

    // There may be another collision at the very end if i==L-2
    if ((i == L - 2) && (S[L - 2] == S[L - 1])) twoCount++;

    // The number of collisions (v) is equal to the number of 2-symbol collisions (twoCount) plus the number of 3-symbol collisions (threeCount)
    result->v = twoCount + threeCount;

    // This is less than or equal to L
    result->tSum = 2 * twoCount + 3 * threeCount;

    // This is less than or equal to 3L
    tSqSum = 4 * twoCount + 9 * threeCount;

    // Shifting over to floating point calculations...
    result->mean = ((double)result->tSum) / ((double)result->v);

    // Note that we calculate a running standard deviation
    // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance
    result->stddev = sqrt((((double)tSqSum) - (((double)result->tSum) * (((double)result->tSum) / ((double)result->v)))) / ((double)(result->v - 1)));

    // colsEstFct is monotonic down, so a lower target is going to result in a higher p (thus more conservative entropy assessment).
    // This operates under the assumption that the mean is normally distributed, which for an IID source seems like a reasonable assumption
    //(this seems like a direct application of the CLT).
    result->meanbound = result->mean - ZALPHA * result->stddev / sqrt((double)result->v);
    // Values less than 2 don't make sense; the mean can't be that low (and these values would correspond to values of p>1).
    if (result->meanbound < 2.0) {
      if (configVerbose > 3) fprintf(stderr, "Collision Estimate: Mean bound reduced under 2 (the minimum possible value). Correcting to 2.\n");
      result->meanbound = 2.0;
    }
    assert(fetestexcept(FE_UNDERFLOW) == 0);
  }

  // Uyen Dinh observed that (with the simpler F function) we can simplify the entire expression much further than in 90B.
  // The whole mess in 90B step 7 reduces to X'-bar = -2p^2 + 2p + 2, which we can solve using the quadratic formula.
  // We only care about the root greater than 0.5, so we only care about the "+" branch.
  // If the meanbound > 2.5, then the roots become complex, so this isn't well defined (and is processed as per the error handling specified in 90B).
  if (result->meanbound < 2.5) {
    result->p = 0.5 + sqrt(1.25 - 0.5 * result->meanbound);
    if (configVerbose > 3) fprintf(stderr, "Collision Estimate: Found p.\n");
  } else {
    result->p = 0.5;
    if (configVerbose > 3) fprintf(stderr, "Collision Estimate: Could Not Find p. Proceeding with the lower bound for p.\n");
  }

  result->entropy = -log2(result->p);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions == FE_UNDERFLOW) {
    fprintf(stderr, "Found an Underflow in collision estimator: (result: %.17g ignored and set to 0.0)\n", result->entropy);
    feclearexcept(FE_UNDERFLOW);
    assert(result->entropy < RELEPSILON);
    result->entropy = 0.0;
  } else if (exceptions != 0) {
    fprintf(stderr, "Math error in Collision estimator: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Underflow ");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  result->done = true;
  return result->entropy;
}

/*6.3.3*/
/*This applies only in the binary case now*/
/*We presume that we get translated data, that is the integers 0 .. 1*/
/*There can be no missing values*/

/*All possibilities for the final pMax can be expressed as a particular form
 * involving the 63rd power of a variable*/
/*This function returns -log2(initial * iter^63 * final)*/
static double chainEnt(double inital, double iter, double final) {
  if ((inital > 0.0) && (iter > 0.0) && (final > 0.0)) {
    return -log2(inital) - 63.0 * log2(iter) - log2(final);
  } else {
    return DBL_INFINITY;
  }
}

double markovEstimate(const statData_t *S, size_t L, struct markovResult *result) {
  double curEst, chainMinEntropy;
  int exceptions;

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  if (L < 2) {
    fprintf(stderr, "Markov Estimate only defined for data samples larger than 1 sample.\n");
    return -1.0;
  }

  if (S != NULL) {
    size_t C_0 = 0;
    size_t C_1;
    size_t C_00 = 0;
    size_t C_10 = 0;
    const statData_t *curp = S;
    statData_t lastSymbol;

    lastSymbol = S[0];

    /*Initialize the counts*/
    for (curp = S + 1; curp < S + L; curp++) {
      statData_t curSymbol = *curp;
      if (lastSymbol == 0) {
        C_0++;
        if (curSymbol == 0) C_00++;
      } else {
        if (curSymbol == 0) C_10++;
      }
      lastSymbol = curSymbol;
    }

    // C_0 is now  the number of 0 bits from S[0] to S[L-2]
    // C_10 is set correctly

    // C_1 is the number of 1 bits from S[0] to S[len-2]
    C_1 = L - 1 - C_0;

    // Note that P_X1 = C_X1 / C_X = (C_X - C_X0)/C_X = 1.0 - C_X0/C_X = 1.0 - P_X0
    if (C_0 > 0) {
      result->T[0][0] = ((double)C_00) / ((double)C_0);
      result->T[0][1] = 1.0 - result->T[0][0];
    } else {
      result->T[0][0] = 0.0;
      result->T[0][1] = 0.0;
    }

    if (C_1 > 0) {
      result->T[1][0] = ((double)C_10) / ((double)C_1);
      result->T[1][1] = 1.0 - result->T[1][0];
    } else {
      result->T[1][0] = 0.0;
      result->T[1][1] = 0.0;
    }

    // account for the last symbol
    if (lastSymbol == 0) C_0++;

    // calculate the probability of a 0 bit (P0) and the probability of a 1 bit (P1)
    result->P0 = (double)C_0 / (double)L;
    result->P1 = 1.0 - result->P0;
  }

  // Now examine each case, and take the smallest assessed entropy
  // Sequence is 00...0
  chainMinEntropy = chainEnt(result->P0, result->T[0][0] * result->T[0][0], result->T[0][0]);

  // Sequence is 0101...01
  curEst = chainEnt(result->P0, result->T[0][1] * result->T[1][0], result->T[0][1]);
  if (chainMinEntropy > curEst) chainMinEntropy = curEst;

  // Sequence is 0111...1
  curEst = chainEnt(result->P0, result->T[1][1] * result->T[1][1], result->T[0][1]);
  if (chainMinEntropy > curEst) chainMinEntropy = curEst;

  // Sequence is 1000...0
  curEst = chainEnt(result->P1, result->T[0][0] * result->T[0][0], result->T[1][0]);
  if (chainMinEntropy > curEst) chainMinEntropy = curEst;

  // Sequence is 1010...10
  curEst = chainEnt(result->P1, result->T[0][1] * result->T[1][0], result->T[1][0]);
  if (chainMinEntropy > curEst) chainMinEntropy = curEst;

  // Sequence is 1111...1
  curEst = chainEnt(result->P1, result->T[1][1] * result->T[1][1], result->T[1][1]);
  if (chainMinEntropy > curEst) chainMinEntropy = curEst;

  // Deal with any floating point problems
  if (chainMinEntropy < 0.0)
    chainMinEntropy = 0.0;
  else if (chainMinEntropy > 128.0)
    chainMinEntropy = 128.0;

  result->phatmax = pow(2.0, -chainMinEntropy);

  result->entropy = chainMinEntropy / 128.0;

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions == FE_UNDERFLOW) {
    fprintf(stderr, "Found an Underflow in Markov estimator: (result: %.17g ignored and set to 0.0)\n", result->entropy);
    feclearexcept(FE_UNDERFLOW);
    assert(result->entropy < RELEPSILON);
    result->entropy = 0.0;
  } else if (exceptions != 0) {
    fprintf(stderr, "Math error in Markov estimator: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  result->done = true;
  return result->entropy;
}

// Compression estimate functions
// 6.3.4
/*Binary inputs only*/
// There is some cleverness associated with this calculation of G; in particular,
// one doesn't need to calculate all the terms independently (they are inter-related!)
// See UL's implementation guidance in the section "Compression Estimate G Function Calculation"
static double compG(double z, size_t blockCount, size_t d) {
  size_t i;
  double Ad1;
  size_t v = blockCount - d;

  struct compensatedState Ai;
  struct compensatedState firstSum;
  double firstSumOut, AiOut;

  // double rawfirstSum;
  long double Bi;
  long double Bterm;
  int exceptions;
  long double ai;
  long double aiScaled;
  double res;

  bool underflowTruncate;

  assert(d > 0);
  assert(blockCount > d);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  // i=2
  initCompensatedSum(&Ai, "Ai", 0);
  initCompensatedSum(&firstSum, "firstSum", 1);

  Bterm = (1.0L - (long double)z);
  // Note: B_1 isn't needed, as a_1 = 0
  // B_2
  Bi = Bterm;

  // Calculate A_{d+1}
  for (i = 2; i <= d; i++) {
    // calculate the a_i term
    ai = log2l((long double)i) * Bi;

    compensatedSum(&Ai, (double)ai);

    // Calculate B_{i+1}
    Bi *= Bterm;
  }

  // Store A_{d+1}
  Ad1 = compensatedSumResult(&Ai);

  underflowTruncate = false;
  // Now calculate A_{blockCount} and the sum of sums term (firstsum)
  for (i = d + 1; i <= blockCount - 1; i++) {
    // calculate the a_i term
    ai = log2l((long double)i) * Bi;

    // Calculate A_{i+1}
    compensatedSum(&Ai, (double)ai);
    // Sum in A_{i+1} into the firstSum

    // Calculate the tail of the sum of sums term (firstsum)
    aiScaled = (long double)(blockCount - i) * ai;
    if ((double)aiScaled > 0.0) {
      compensatedSum(&firstSum, (double)aiScaled);
    } else {
      if (configVerbose > 4) fprintf(stderr, "Expected compG underflow in calculating sum-of-sums in round %zu\n", i);
      underflowTruncate = true;
      break;
    }

    // Calculate B_{i+1}
    Bi *= Bterm;
  }

  // Ai now contains A_{blockCount} and firstsum contains the tail
  // finalize the calculation of firstsum
  compensatedSum(&firstSum, ((double)(blockCount - d)) * Ad1);

  // Calculate A_{blockCount+1}
  if (!underflowTruncate) {
    ai = log2l((long double)blockCount) * Bi;
    compensatedSum(&Ai, (double)ai);
  }

  firstSumOut = compensatedSumResult(&firstSum);
  AiOut = compensatedSumResult(&Ai);

  if (configVerbose > 4) {
    fprintf(stderr, "firstSum: %.17g, A_{blockCount+1}: %.17g\n", firstSumOut, AiOut);
  }

  // the result
  res = 1 / (double)v * z * (z * firstSumOut + (AiOut - Ad1));

  // clean up and check for floating point errors
  delCompensatedSum(&Ai);
  delCompensatedSum(&firstSum);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO);
  if (exceptions != 0) {
    fprintf(stderr, "CompG math error: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  exceptions = fetestexcept(FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions != 0) {
    if (exceptions & FE_OVERFLOW) {
      if (configVerbose > 3) {
        fprintf(stderr, "Found an overflow in compG\n");
      }
      feclearexcept(FE_OVERFLOW);
    }
    if (exceptions & FE_UNDERFLOW) {
      if (configVerbose > 4) {
        fprintf(stderr, "Found an Underflow in compG\n");
      }
      feclearexcept(FE_UNDERFLOW);
    }
  }

  if (configVerbose > 4) {
    fprintf(stderr, "G(z=%.17g, blockCount=%zu, d=%zu) = %.17g\n", z, blockCount, d, res);
  }
  return res;
}

// params is a size_t array, consisting of k (k=2^b), blockCount, and d
static double compEstFct(double p, const size_t *params) {
  double k;

  k = (double)params[0];
  assert(params[0] > 1);
  assert(p < 1.0);
  assert(p >= 1.0 / k);
  return compG(p, params[1], params[2]) + (k - 1.0) * compG((1.0 - p) / (k - 1.0), params[1], params[2]);
}

// b == 6 in our code
static statData_t maurerAccess(const statData_t *S, size_t index, size_t b) {
  statData_t out;
  size_t i;

  assert(b > 0);
  assert(b <= STATDATA_BITS);

  out = 0;
  for (i = 0; i < b; i++) {
    out = (statData_t)((out << 1) | (S[b * index + i] & 1));
  }

  return out;
}

static void maurerStats(const statData_t *S, size_t L, size_t b, size_t d, double *results) {
  statData_t curdata;
  double mean, meanofsquares, meandelta;
  double elem;
  double stddev;
  size_t j;
  size_t Lp;
  size_t v;
  double c;
  size_t k;
  struct compensatedState maurerSum;
  struct compensatedState maurerSumOfSquares;
  size_t *dict;
  size_t *D;

  assert(S != NULL);
  assert(results != NULL);
  assert(b > 0);
  assert(b <= STATDATA_BITS);

  initCompensatedSum(&maurerSum, "maurerSum", 2);
  initCompensatedSum(&maurerSumOfSquares, "maurerSumOfSquares", 3);

  k = 1U << b;
  Lp = L / b;
  v = Lp - d;

  assert(k >= 2);
  assert(Lp > d);
  assert(Lp >= k);

  // Allocate and zero
  if ((dict = calloc(k, sizeof(size_t))) == NULL) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }

  if ((D = calloc(v, sizeof(size_t))) == NULL) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }

  for (j = 0; j < d; j++) {
    curdata = maurerAccess(S, d - j - 1, b);
    if (dict[curdata] == 0) {
      dict[curdata] = d - j - 1;
    }
  }

  for (j = d; j < Lp; j++) {
    curdata = maurerAccess(S, j, b);
    if (dict[curdata] != 0) {
      // This is a delta
      D[j - d] = j - dict[curdata];
    } else {
      // The literal index is here.
      D[j - d] = j + 1;
    }
    dict[curdata] = j;
  }

  for (j = 0; j < v; j++) {
    assert(D[j] != 0);
    elem = log2((double)D[j]);
    compensatedSum(&maurerSum, elem);
    compensatedSum(&maurerSumOfSquares, elem * elem);
  }

  meanofsquares = compensatedSumResult(&maurerSumOfSquares) / ((double)(v - 1));
  mean = compensatedSumResult(&maurerSum) / (double)v;

  delCompensatedSum(&maurerSumOfSquares);
  delCompensatedSum(&maurerSum);

  free(D);
  D = NULL;
  free(dict);
  dict = NULL;

  c = 0.5907;

  /*
  b = floor(log2((double)(k-1))) + 1.0;
  c=0.7-0.8/b+(4.0+32/b)*pow((double)v,-3/b)/15;*/

  stddev = c * sqrt((meanofsquares - mean * mean));

  meandelta = ZALPHA * stddev / sqrt((double)v);

  results[0] = (double)mean;
  results[1] = (double)stddev;
  results[2] = (double)meandelta;
}

/*6.3.4 Compression estimate*/
/*Binary inputs only*/
double compressionEstimate(const statData_t *S, size_t L, struct compResult *result) {
  /*Todo: calculate d so that the expected number of values in the dictionary is at least 10*/
  /*Todo: verify v is sufficiently large*/
  const size_t d = 1000;
  size_t params[3];
  double results[3];
  int exceptions;
  const size_t b = 6;
  const size_t k = 1U << b;  // k=2^b

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  if (S != NULL) {
    if (L <= b * 1000) {
      fprintf(stderr, "Compression Estimate: Insufficient Number of Samples.\n");
      return (-1.0);
    }

    // Moving over to floating point
    maurerStats(S, L, b, d, results);
    assert(fetestexcept(FE_UNDERFLOW) == 0);

    // results[0] is the mean
    // results[2] is the expected delta for a 99% confidence interval
    // compEstFct is monotonic down, so a lower target is going to result in a higher p (thus more conservative entropy assessment).
    // This operates under the assumption that the mean is normally distributed, which for an IID source seems like a reasonable assumption
    //(this seems like a direct application of the CLT).
    result->mean = results[0];
    result->meanbound = results[0] - results[2];
    result->stddev = results[1];
    result->L = L;
  }

  params[0] = k;
  params[1] = L / b;
  params[2] = d;

  // This is expected to underflow for many values (perhaps most values!)
  if (compEstFct(1.0 / ((double)k), params) > result->meanbound) {
    result->p = monotonicBinarySearch(compEstFct, 1.0 / ((double)k), 1.0, result->meanbound, params, true);
  } else {
    // We are required to return 1 if there is no match
    result->p = -1.0;
  }

  if (fetestexcept(FE_UNDERFLOW) != 0) {
    if (configVerbose > 3) fprintf(stderr, "(Expected) Underflow encountered in monotonicBinarySearch. Ignoring it.\n");
    feclearexcept(FE_UNDERFLOW);
  }

  if (result->p > 1.0) {
    result->p = 1.0;
  }

  if (result->p > 1.0 / (double)k) {
    if (configVerbose > 3) fprintf(stderr, "Compression Estimate: Found p.\n");
    result->entropy = -log2(result->p) / (double)b;
  } else {
    if (configVerbose > 3) fprintf(stderr, "Compression Estimate: Could Not Find p. Proceeding with the lower bound for p.\n");
    result->entropy = 1.0;
    result->p = 1.0 / (double)k;
  }

  // Checking for underflow for the log operation, or other issues for whatever.
  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions == FE_UNDERFLOW) {
    fprintf(stderr, "Found an Underflow in entropy calculation of Compression estimator: (result: %.17g ignored and set to 0.0)\n", result->entropy);
    feclearexcept(FE_UNDERFLOW);
    assert(result->entropy < RELEPSILON);
    result->entropy = 0.0;
  } else if (exceptions != 0) {
    fprintf(stderr, "Math error in Compression estimator: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Underflow ");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  result->done = true;
  return result->entropy;
}

/*6.3.5 and 6.3.6*/
/* Based on the algorithm outlined by Aaron Kaufer
 * This is described here:
 * http://www.untruth.org/~josh/sp80090b/Kaufer%20Further%20Improvements%20for%20SP%20800-90B%20Tuple%20Counts.pdf
 */
static void SAalgs32(const statData_t *data, size_t n, size_t k, struct SAresult *result) {
  saidx_t *SA;  // each value is at most n-1
  saidx_t *L;  // each value is at most n-1
  saidx_t *LCP;  // each value is at most n-1
  saidx_t *I;  // 0 <= I[i] <= v+2 <= n+1

  saidx_t j;  // 0 <= j <= v+1 <= n
  saidx_t c;  // contains a count from A. c <= n
  saidx_t t;  // Takes values from LCP array. 0 <= t < n
  saidx_t v;  // The length of the LRS. -1 <= v <= n-1
  saidx_t u;  // The length of a string: 0 <= u <= v+1 <= n
  saidx_t *Q;  // Contains an accumulation of positive counts 1 <= Q[i] <= n
  saidx_t *A;  // Contains an accumulation of positive counts 0 <= A[i] <= n
  long double Pmax;
  long double pu;
  uint64_t *S;  // Each value 0 <= S[i] < n^3
  int exceptions;

  assert(n > 0);
  assert(k > 0);
  assert(data != NULL);
  assert(n < SAIDX_MAX);

  // We ultimately need to be able to compute using values of the scale n choose 2
  // Note that n choose 2 is just (n)(n-1)/2.
  // We can fit the numerator of this expression in a size_t type variable variable works iff
  // SIZE_MAX >= n * (n-1) iff
  // SIZE_MAX / n >= n-1, which is is surely true if
  // floor(SIZE_MAX / n) >= n-1.
  assert((SIZE_MAX / n) >= (n-1)); // (mult assert)

  assert(n <= SAIDX_MAX - 1);
  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  /*First, allocate the necessary structures*/
  if((SA = (saidx_t *)malloc((n + 1) * sizeof(saidx_t)))==NULL) {
    perror("Cannot allocate memory for SA array.\n");
    exit(EX_OSERR);
  }

  if((LCP = (saidx_t *)malloc((n + 2) * sizeof(saidx_t)))==NULL) {
    free(SA);
    perror("Cannot allocate memory for LCP array.\n");
    exit(EX_OSERR);
  }

  if (configVerbose > 3) {
    fprintf(stderr, "Calculate SA/LCP, size: %zu, symbols: %zu\n", n, k);
  }
  calcSALCP(data, n, k, SA, LCP);
  // to conform with Kaufer's conventions
  assert(LCP[1] == 0);
  LCP[n + 1] = 0;
  L = LCP + 1;

  // Find the length of the LRS, v
  v = 0;
  for (size_t i = 0; i <= n; i++) {
    if (L[i] > v) v = L[i];
  }
  assert((v > 0) && ((size_t)v < n));

  result->v = (size_t)v;
  if (configVerbose > 3) {
    fprintf(stderr, "LRS: v = %d\n", v);
  }

  // If the LRS length is bigger than L/256, this could take a while...
  if(((size_t)v> (n/256)) && (configVerbose > 0)) {
    // One way the data can be very flawed is a long repeated substring.
    // In this instance, the runtime can become unmanageably long, so it
    // could be useful to tell the tester about this condition before the
    // test completes.
    //
    // We can create a *upper* bound for the *entropy* by
    // creating a *lower* bound for the probability.
    //
    // In this estimator, P_W is an unbiased estimator of the W-tuple
    // collision probability. We know that p-hat >= P_v^(1/v) >= 1/((L-W+1)*(L-W))
    // (as there are at least 2 instances of the LRS).
    //
    // We print out this estimate to allow the tester to exit out if the result
    // is essentially 0.
    //
    // Note that (c choose 2) is just (c)(c-1)/2.
    long double Pwbound = powl(1.0L / (((long double)(n - (size_t)v + 1))*((long double)(n-(size_t)v))/2.0L), 1.0L/((long double)v));
    long double pubound = fminl(1.0L, Pwbound + ZALPHA_L * sqrtl(Pwbound*(1.0L-Pwbound)/((long double)(n-1))));
    long double lrsminentbound = - log2l(pubound);
    fprintf(stderr, "LRS length is large as compared to the dataset, so the LRS estimator may take a while. A LRS result upper bound is approximately %Lg.\n", lrsminentbound);
  }

  if((Q=malloc(((size_t)v + 1) * sizeof(saidx_t)))==NULL) {
    free(SA);
    free(LCP);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  if((A = calloc((size_t)v + 2, sizeof(saidx_t)))==NULL) {
    free(SA);
    free(LCP);
    free(Q);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  // j takes the value 0 to v+1
  // Note that I is indexed by at most j+1. (so I[v+2] should work)
  // I stores indices of A, and there are only v+2 of these
  if((I = calloc((size_t)v + 3, sizeof(saidx_t)))==NULL) {
    free(SA);
    free(LCP);
    free(Q);
    free(A);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  for (j = 0; j <= v; j++) Q[j] = 1;

  j = 0;
  // O(nv) operations
  for (size_t i = 1; i <= n; i++) {  // n iterations
    c = 0;
    // Note L[0] is already verified to be 0
    assert(L[i] >= 0);

    if (L[i] < L[i - 1]) {
      t = L[i - 1];
      assert(j > 0);
      j--;
      assert(j <= v);

      while (t > L[i]) {  // At most v
        assert((t > 0) && (t <= v));
        if ((j > 0) && (I[j] == t)) {
          /* update count for non-zero entry of A */
          A[I[j]] += A[I[j + 1]];
          A[I[j + 1]] = 0;
          j--;
        }

        if (Q[t] >= A[I[j + 1]] + 1) {
          /*
           * Q[t] is at least as large as current count,
           * and since Q[t] <= Q[t-1] <= ... <= Q[1],
           * there is no need to check zero entries of A
           * until next non-zero entry
           */
          if (j > 0) {
            /* skip to next non-zero entry of A */
            t = I[j];
          } else {
            /*
             * no more non-zero entries of A,
             * so skip to L[i] (terminate while loop)
             */
            t = L[i];
          }
        } else {
          /* update Q[t] with new maximum count */
          Q[t--] = A[I[j + 1]] + 1;
        }
      }

      c = A[I[j + 1]]; /* store carry over count */
      A[I[j + 1]] = 0;
    }

    if (L[i] > 0) {
      if ((j < 1) || (I[j] < L[i])) {
        /* insert index of next non-zero entry of A */
        assert(j <= v);
        I[++j] = L[i];
      }
      A[I[j]] += c + 1; /* update count for t = I[j] = L[i] */
    }
  }

  // Calculate u
  for (u = 1; (u <= v) && (Q[u] >= 35); u++)
    ;

  assert(u > 0);
  assert(((u == v + 1) || ((u <= v) && (Q[u] < 35))));
  assert(((u == 1) || (Q[u - 1] >= 35)));
  // At this point u is correct
  result->done = true;
  result->u = (size_t)u;

  if (configVerbose > 3) fprintf(stderr, "t-Tuple: u = %d\n", u);

  // at this point, Q is completely calculated.
  // Q is the count of (one of) the most common j-tuples
  /*Calculate the various Pmax[i] values. We need not save the actual values, only the largest*/
  Pmax = -1.0L;
  for (j = 1; j < u; j++) {
    long double curP = ((long double)(Q[j])) / ((long double)((int64_t)n - j + 1));
    long double curPMax = powl(curP, 1.0L / (long double)j);

    assert(Q[j] >= 35);
    if (configVerbose > 3) {
      fprintf(stderr, "t-Tuple Estimate: Q[%d] = %d\n", j, Q[j]);
      fprintf(stderr, "t-Tuple Estimate: P[%d] = %.22Lg\n", j, curP);
      fprintf(stderr, "t-Tuple Estimate: P_max[%d] = %.22Lg\n", j, curPMax);
    }
    if (curPMax > Pmax) {
      Pmax = curPMax;
    }
  }

  if (Pmax > 0.0L) {
    result->tTuplePmax = (double)Pmax;
    // Note, this is a local guess at a confidence interval, under the assumption that the inferred most probable symbol proportion is distributed
    // as per the normal distribution. This parameter is a maximum of a sequence of inferred parameters. The individual inferred parameters may
    // be thought of as a sort of inferred probability of the MLS (which is, as mentioned above, not expected to be normal, even with IID data).
    // Even if these individual inferred parameters were themselves normal, taking the maximum of several such parameters won't be.

    pu = Pmax + ZALPHA_L * sqrtl(Pmax * (1.0L - Pmax) / ((long double)(n - 1)));
    if (pu > 1.0L) {
      pu = 1.0L;
    }
    result->tTuplePu = (double)pu;

    result->tTupleEntropy = (double)-log2l(pu);
    result->tTupleDone = true;
  } else {
    result->tTuplePmax = -1.0;
    result->tTuplePu = -1.0;
    result->tTupleEntropy = -1.0;
    result->tTupleDone = false;
  }

  if (v < u) {
    fprintf(stderr, "v < u, so we skip the lrs test.\n");
    free(SA);
    free(LCP);
    free(Q);
    free(A);
    free(I);
    result->lrsEntropy = -1.0;
    result->lrsPmax = -1.0;
    result->lrsPu = -1.0;
    result->lrsDone = false;

    return;
  }

  if ((S = calloc((size_t)(v + 1), sizeof(uint64_t))) == NULL) {
    perror("Cannot allocate memory to sum P_W.\n");
    exit(EX_OSERR);
  }
  memset(A, 0, sizeof(saidx_t) * ((size_t)v + 2));

  // O(nv) operations
  for (size_t i = 1; i <= n; i++) {  // n iterations
    if ((L[i - 1] >= u) && (L[i] < L[i - 1])) {
      saidx_t b = L[i];

      // A[u] stores the number of u-length tuples. We need to eventually clear down to A[u]=A[b+1].
      if (b < u) b = u - 1;

      for (t = L[i - 1]; t > b; t--) {  // at most v
        A[t] += A[t + 1];
        A[t + 1] = 0;

        assert(A[t] >= 0);
        // update sum
        // Note that (c choose 2) is just (c)(c-1)/2.
        // The numerator of this expression is necessarily even
        // Dividing an even quantity by 2 is the same as right shifting by 1.
        // Check for overflows when adding to Psum element (unsigned 64 bit integers)
        // The logic here is just
        // S[t] += (((uint64_t)(A[t]+1) * (uint64_t)(A[t]))/2); /* update sum */
        // A[t] is the number of times the value _repeats_, so the total count of such symbols is A[t]+1.
        // As such, we want to add (A[t] + 1 choose 2) = (A[t]+1)*A[t]/2.
        // Note, A[t] < n, so the assert marked "(mult assert)" tells us that the multiplication won't rollover.
        safeAdduint64(S[t], (((uint64_t)A[t]) * ((uint64_t)A[t] + 1U)) >> 1, &S[t]);
      }
      if (b >= u) A[b] += A[b + 1]; /* carry over count for t = L[i] */
      A[b + 1] = 0;
    }
    if (L[i] >= u) A[L[i]]++; /* update count for t = L[i] */
  }

  Pmax = 0.0L;
  for (j = u; j <= v; j++) {
    // Note:
    // j>=u>0, so (n-j) * (n-j+1) <= (n-1)*(n)
    // By the assert marked "(mult assert)", floor(SIZE_MAX / n) >= (n-1), so the multiplication won't rollover.
    size_t choices = (((n - (size_t)j) * (n - (size_t)j + 1U)) >> 1);
    long double curP, curPMax;
    assert(S[j] <= choices);

    curP = ((long double)S[j]) / (long double)choices;
    curPMax = powl(curP, 1.0L / ((long double)j));
    // curP is now an estimate for the probability of collision across all j-tuples.
    // This was calculated using an unbiased estimator for the _distribution's_ 2-moment;
    // see "Improvised Estimation of Collision Entropy..." by Skorski, equation (1)
    // or "The Complexity of Estimating Rényi Entropy" by Acharya, Orlitsky, Suresh and Tyagi Section 1.5.
    if (configVerbose > 3) {
      fprintf(stderr, "LRS Estimate: P_%d = %.22Lg ( %zu / %zu )\n", j, curP, S[j], choices);
      fprintf(stderr, "LRS Estimate: P_{max,%d} = %.22Lg\n", j, curPMax);
    }
    if (Pmax < curPMax) {
      Pmax = curPMax;
    }
  }

  // Note, this is a local guess at a confidence interval, under the assumption that the inferred most probable symbol proportion is distributed
  // as per the normal distribution. This parameter is a maximum of a sequence of inferred parameters. The individual inferred parameters may
  // be thought of as a sort of inferred probability of the MLS (which is, as mentioned above, not expected to be normal, even with IID data).
  // Even if these individual inferred parameters were themselves normal, taking the maximum of several such parameters won't be.
  result->lrsPmax = (double)Pmax;
  pu = Pmax + ZALPHA_L * sqrtl(Pmax * (1.0L - Pmax) / ((long double)(n - 1)));
  if (pu > 1.0L) {
    pu = 1.0L;
  }
  result->lrsPu = (double)pu;

  result->lrsEntropy = (double)-log2l(pu);
  result->lrsDone = true;

  free(S);
  S = NULL;
  free(I);
  I = NULL;
  free(SA);
  SA = NULL;
  free(LCP);
  L = NULL;
  free(Q);
  Q = NULL;
  free(A);
  A = NULL;

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions != 0) {
    fprintf(stderr, "Math error in Suffix-Array based estimators: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  return;
}

static void SAalgs64(const statData_t *data, size_t n, size_t k, struct SAresult *result) {
  saidx64_t *SA=NULL;// each value is at most n-1
  saidx64_t *L=NULL;  // each value is at most n-1
  saidx64_t *LCP=NULL;  // each value is at most n-1
  saidx64_t *I=NULL;  // 0 <= I[i] <= v+2 <= n+1

  saidx64_t j;  // 0 <= j <= v+1 <= n
  saidx64_t c;  // contains a count from A. c <= n
  saidx64_t t;  // Takes values from LCP array. 0 <= t < n
  saidx64_t v;  // The length of the LRS. -1 <= v <= n-1
  saidx64_t u;  // The length of a string: 0 <= u <= v+1 <= n
  saidx64_t *Q=NULL;  // Contains an accumulation of positive counts 1 <= Q[i] <= n
  saidx64_t *A=NULL;  // Contains an accumulation of positive counts 0 <= A[i] <= n
  long double Pmax;
  long double pu;
  uint128_t *S=NULL;  // Each value 0 <= S[i] < n^3
  int exceptions;

  assert(n > 0);
  assert(k > 0);
  assert(data != NULL);
  assert(n < SAIDX64_MAX);

  // We ultimately need to be able to compute using values of the scale n choose 2
  // Note that n choose 2 is just (n)(n-1)/2.
  // We can fit the numerator of this expression in a size_t type variable variable works iff
  // UINT128_MAX >= n * (n-1) iff
  // UINT128_MAX / n >= n-1, which is is surely true if
  // floor(UINT128_MAX / n) >= n-1.
  assert((UINT128_MAX / n) >= (n-1)); // (mult assert)

  assert(n <= SAIDX64_MAX - 1);
  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  /*First, allocate the necessary structures*/
  if((SA = (saidx64_t *)malloc((n + 1) * sizeof(saidx64_t)))==NULL) {
    perror("Cannot allocate memory for SA array.\n");
    exit(EX_OSERR);
  }

  if((LCP = (saidx64_t *)malloc((n + 2) * sizeof(saidx64_t)))==NULL) {
    free(SA);
    perror("Cannot allocate memory for LCP array.\n");
    exit(EX_OSERR);
  }

  if (configVerbose > 3) {
    fprintf(stderr, "Calculate SA/LCP, size: %zu, symbols: %zu\n", n, k);
  }
  calcSALCP64(data, n, k, SA, LCP);
  // to conform with Kaufer's conventions
  assert(LCP[1] == 0);
  LCP[n + 1] = 0;
  L = LCP + 1;

  // Find the length of the LRS, v
  v = 0;
  for (size_t i = 0; i <= n; i++) {
    if (L[i] > v) v = L[i];
  }
  assert((v > 0) && ((size_t)v < n));

  result->v = (size_t)v;
  if (configVerbose > 3) {
    fprintf(stderr, "LRS: v = %" PRId64 "\n", v);
  }

  // If the LRS length is bigger than L/256, this could take a while...
  if(((size_t)v> (n/256)) && (configVerbose > 0)) {
    // One way the data can be very flawed is a long repeated substring.
    // In this instance, the runtime can become unmanageably long, so it
    // could be useful to tell the tester about this condition before the
    // test completes.
    //
    // We can create a *upper* bound for the *entropy* by
    // creating a *lower* bound for the probability.
    //
    // In this estimator, P_W is an unbiased estimator of the W-tuple
    // collision probability. We know that p-hat >= P_v^(1/v) >= 1/((L-W+1)*(L-W))
    // (as there are at least 2 instances of the LRS).
    //
    // We print out this estimate to allow the tester to exit out if the result
    // is essentially 0.
    //
    // Note that (c choose 2) is just (c)(c-1)/2.
    long double Pwbound = powl(1.0L / (((long double)(n - (size_t)v + 1))*((long double)(n-(size_t)v))/2.0L), 1.0L/((long double)v));
    long double pubound = fminl(1.0L, Pwbound + ZALPHA_L * sqrtl(Pwbound*(1.0L-Pwbound)/((long double)(n-1))));
    long double lrsminentbound = - log2l(pubound);
    fprintf(stderr, "LRS length is large as compared to the dataset, so the LRS estimator may take a while. A LRS result upper bound is approximately %Lg.\n", lrsminentbound);
  }

  if((Q=malloc(((size_t)v + 1) * sizeof(saidx64_t)))==NULL) {
    free(SA);
    free(LCP);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  if((A = calloc((size_t)v + 2, sizeof(saidx64_t)))==NULL) {
    free(SA);
    free(LCP);
    free(Q);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  // j takes the value 0 to v+1
  // Note that I is indexed by at most j+1. (so I[v+2] should work)
  // I stores indices of A, and there are only v+2 of these
  if((I = calloc((size_t)v + 3, sizeof(saidx64_t)))==NULL) {
    free(SA);
    free(LCP);
    free(Q);
    free(A);
    perror("Cannot allocate memory for state data.\n");
    exit(EX_OSERR);
  }

  for (j = 0; j <= v; j++) Q[j] = 1;

  j = 0;
  // O(nv) operations
  for (size_t i = 1; i <= n; i++) {  // n iterations
    c = 0;
    // Note L[0] is already verified to be 0
    assert(L[i] >= 0);

    if (L[i] < L[i - 1]) {
      t = L[i - 1];
      assert(j > 0);
      j--;
      assert(j <= v);

      while (t > L[i]) {  // At most v
        assert((t > 0) && (t <= v));
        if ((j > 0) && (I[j] == t)) {
          /* update count for non-zero entry of A */
          A[I[j]] += A[I[j + 1]];
          A[I[j + 1]] = 0;
          j--;
        }

        if (Q[t] >= A[I[j + 1]] + 1) {
          /*
           * Q[t] is at least as large as current count,
           * and since Q[t] <= Q[t-1] <= ... <= Q[1],
           * there is no need to check zero entries of A
           * until next non-zero entry
           */
          if (j > 0) {
            /* skip to next non-zero entry of A */
            t = I[j];
          } else {
            /*
             * no more non-zero entries of A,
             * so skip to L[i] (terminate while loop)
             */
            t = L[i];
          }
        } else {
          /* update Q[t] with new maximum count */
          Q[t--] = A[I[j + 1]] + 1;
        }
      }

      c = A[I[j + 1]]; /* store carry over count */
      A[I[j + 1]] = 0;
    }

    if (L[i] > 0) {
      if ((j < 1) || (I[j] < L[i])) {
        /* insert index of next non-zero entry of A */
        assert(j <= v);
        I[++j] = L[i];
      }
      A[I[j]] += c + 1; /* update count for t = I[j] = L[i] */
    }
  }

  // Calculate u
  for (u = 1; (u <= v) && (Q[u] >= 35); u++)
    ;

  assert(u > 0);
  assert(((u == v + 1) || ((u <= v) && (Q[u] < 35))));
  assert(((u == 1) || (Q[u - 1] >= 35)));
  // At this point u is correct
  result->done = true;
  result->u = (size_t)u;

  if (configVerbose > 3) fprintf(stderr, "t-Tuple: u = %" PRId64 "\n", u);

  // at this point, Q is completely calculated.
  // Q is the count of (one of) the most common j-tuples
  /*Calculate the various Pmax[i] values. We need not save the actual values, only the largest*/
  Pmax = -1.0L;
  for (j = 1; j < u; j++) {
    long double curP = ((long double)(Q[j])) / ((long double)((int64_t)n - j + 1));
    long double curPMax = powl(curP, 1.0L / (long double)j);

    assert(Q[j] >= 35);
    if (configVerbose > 3) {
      fprintf(stderr, "t-Tuple Estimate: Q[%" PRId64  "] = %" PRId64  "\n", j, Q[j]);
      fprintf(stderr, "t-Tuple Estimate: P[%" PRId64 "] = %.22Lg\n", j, curP);
      fprintf(stderr, "t-Tuple Estimate: P_max[%" PRId64 "] = %.22Lg\n", j, curPMax);
    }
    if (curPMax > Pmax) {
      Pmax = curPMax;
    }
  }

  if (Pmax > 0.0L) {
    result->tTuplePmax = (double)Pmax;
    // Note, this is a local guess at a confidence interval, under the assumption that the inferred most probable symbol proportion is distributed
    // as per the normal distribution. This parameter is a maximum of a sequence of inferred parameters. The individual inferred parameters may
    // be thought of as a sort of inferred probability of the MLS (which is, as mentioned above, not expected to be normal, even with IID data).
    // Even if these individual inferred parameters were themselves normal, taking the maximum of several such parameters won't be.

    pu = Pmax + ZALPHA_L * sqrtl(Pmax * (1.0L - Pmax) / ((long double)(n - 1)));
    if (pu > 1.0L) {
      pu = 1.0L;
    }
    result->tTuplePu = (double)pu;

    result->tTupleEntropy = (double)-log2l(pu);
    result->tTupleDone = true;
  } else {
    result->tTuplePmax = -1.0;
    result->tTuplePu = -1.0;
    result->tTupleEntropy = -1.0;
    result->tTupleDone = false;
  }

  if (v < u) {
    fprintf(stderr, "v < u, so we skip the lrs test.\n");
    free(SA);
    free(LCP);
    free(Q);
    free(A);
    free(I);
    result->lrsEntropy = -1.0;
    result->lrsPmax = -1.0;
    result->lrsPu = -1.0;
    result->lrsDone = false;

    return;
  }

  if ((S = calloc((size_t)(v + 1), sizeof(uint128_t))) == NULL) {
    perror("Cannot allocate memory to sum P_W.\n");
    exit(EX_OSERR);
  }
  memset(A, 0, sizeof(saidx64_t) * ((size_t)v + 2));

  // O(nv) operations
  for (size_t i = 1; i <= n; i++) {  // n iterations
    if ((L[i - 1] >= u) && (L[i] < L[i - 1])) {
      saidx64_t b = L[i];

      // A[u] stores the number of u-length tuples. We need to eventually clear down to A[u]=A[b+1].
      if (b < u) b = u - 1;

      for (t = L[i - 1]; t > b; t--) {  // at most v
        A[t] += A[t + 1];
        A[t + 1] = 0;

        assert(A[t] >= 0);
        // update sum
        // Note that (c choose 2) is just (c)(c-1)/2.
        // The numerator of this expression is necessarily even
        // Dividing an even quantity by 2 is the same as right shifting by 1.
        // Check for overflows when adding to Psum element (unsigned 64 bit integers)
        // The logic here is just
        // S[t] += (((uint64_t)(A[t]+1) * (uint64_t)(A[t]))/2); /* update sum */
        // A[t] is the number of times the value _repeats_, so the total count of such symbols is A[t]+1.
        // As such, we want to add (A[t] + 1 choose 2) = (A[t]+1)*A[t]/2.
        // Note, A[t] < n, so the assert marked "(mult assert)" tells us that the multiplication won't rollover.
        safeAdduint128(S[t], (((uint128_t)A[t]) * ((uint128_t)A[t] + 1ULL)) >> 1, &S[t]);
      }
      if (b >= u) A[b] += A[b + 1]; /* carry over count for t = L[i] */
      A[b + 1] = 0;
    }
    if (L[i] >= u) A[L[i]]++; /* update count for t = L[i] */
  }

  Pmax = 0.0L;
  for (j = u; j <= v; j++) {
    // Note:
    // j>=u>0, so (n-j) * (n-j+1) <= (n-1)*(n)
    // By the assert marked "(mult assert)", floor(UINT128_MAX / n) >= (n-1), so the multiplication won't rollover.
    uint128_t choices = (((n - (uint128_t)j) * (n - (uint128_t)j + 1U)) >> 1);
    long double curP, curPMax;
    assert(S[j] <= choices);

    curP = ((long double)S[j]) / (long double)choices;
    curPMax = powl(curP, 1.0L / ((long double)j));
    // curP is now an estimate for the probability of collision across all j-tuples.
    // This was calculated using an unbiased estimator for the _distribution's_ 2-moment;
    // see "Improvised Estimation of Collision Entropy..." by Skorski, equation (1)
    // or "The Complexity of Estimating Rényi Entropy" by Acharya, Orlitsky, Suresh and Tyagi Section 1.5.
    if (configVerbose > 3) {
      char buffer1[40];
      char buffer2[40];
      fprintf(stderr, "LRS Estimate: P_%" PRId64 " = %.22Lg ( %s / %s )\n", j, curP, uint128ToString(S[j], buffer1), uint128ToString(choices, buffer2));
      fprintf(stderr, "LRS Estimate: P_{max,%" PRId64 "} = %.22Lg\n", j, curPMax);
    }
    if (Pmax < curPMax) {
      Pmax = curPMax;
    }
  }

  // Note, this is a local guess at a confidence interval, under the assumption that the inferred most probable symbol proportion is distributed
  // as per the normal distribution. This parameter is a maximum of a sequence of inferred parameters. The individual inferred parameters may
  // be thought of as a sort of inferred probability of the MLS (which is, as mentioned above, not expected to be normal, even with IID data).
  // Even if these individual inferred parameters were themselves normal, taking the maximum of several such parameters won't be.
  result->lrsPmax = (double)Pmax;
  pu = Pmax + ZALPHA_L * sqrtl(Pmax * (1.0L - Pmax) / ((long double)(n - 1)));
  if (pu > 1.0L) {
    pu = 1.0L;
  }
  result->lrsPu = (double)pu;

  result->lrsEntropy = (double)-log2l(pu);
  result->lrsDone = true;

  free(S);
  S = NULL;
  free(I);
  I = NULL;
  free(SA);
  SA = NULL;
  free(LCP);
  L = NULL;
  free(Q);
  Q = NULL;
  free(A);
  A = NULL;

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions != 0) {
    fprintf(stderr, "Math error in Suffix-Array based estimators: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  return;
}

void SAalgs(const statData_t *data, size_t n, size_t k, struct SAresult *result) {
    if(n < INT_MAX) {
      SAalgs32(data, n, k, result);
    } else {
      SAalgs64(data, n, k, result);
    }
}

struct multiMCWPredictorState {
  size_t windowSize;
  size_t *counts;
  size_t *symbolLastSeen;
  statData_t prediction;
  size_t correctPredictions;
  size_t k;
};

static struct multiMCWPredictorState *initMCWPredictor(const statData_t *S, size_t L, size_t k, size_t inWindowSize) {
  struct multiMCWPredictorState *out;
  size_t j;

  if (inWindowSize >= L) {
    return NULL;
  }

  if ((out = malloc(sizeof(struct multiMCWPredictorState))) == NULL) {
    perror("Can't allocate predictor (1)");
    exit(EX_OSERR);
  }

  if ((out->counts = malloc(sizeof(size_t) * k)) == NULL) {
    perror("Can't allocate predictor (2)");
    exit(EX_OSERR);
  }

  if ((out->symbolLastSeen = malloc(sizeof(size_t) * k)) == NULL) {
    perror("Can't allocate predictor (2)");
    exit(EX_OSERR);
  }

  // Initialize the multiMCWPredictorState structure
  out->windowSize = inWindowSize;
  for (j = 0; j < k; j++) {
    out->counts[j] = 0;
    out->symbolLastSeen[j] = 0;
  }
  out->prediction = 0;
  out->correctPredictions = 0;
  out->k = k;

  out->counts[S[0]] = 1;
  out->prediction = S[0];
  out->symbolLastSeen[S[0]] = 0;
  for (j = 1; j < out->windowSize; j++) {
    out->counts[S[j]]++;
    if (out->counts[S[j]] >= out->counts[out->prediction]) {
      out->prediction = S[j];
    }
    out->symbolLastSeen[S[j]] = j;
  }

  return (out);
}

static void delMultiMCWPredictor(struct multiMCWPredictorState *in) {
  if (in != NULL) {
    if (in->counts != NULL) {
      free(in->counts);
      in->counts = NULL;
    }
    if (in->symbolLastSeen != NULL) {
      free(in->symbolLastSeen);
      in->symbolLastSeen = NULL;
    }
    free(in);
  }
}

static void updateMultiMCWPrediction(struct multiMCWPredictorState *in) {
  size_t j;

  assert(in != NULL);
  assert(in->counts != NULL);
  assert(in->symbolLastSeen != NULL);
  assert(in->k - 1 <= STATDATA_MAX);

  for (j = 0; j < in->k; j++) {
    if ((in->counts[j] > in->counts[in->prediction]) || ((in->counts[j] == in->counts[in->prediction]) && (in->symbolLastSeen[j] > in->symbolLastSeen[in->prediction]))) {
      in->prediction = (statData_t)j;
    }
  }
}

// Note that the domain is p in [0,1)
static double predictionEstFct(double pin, const size_t *params) {
  long double q;
  long double p;
  long double N;
  uint32_t r;
  long double x;
  size_t j;
  long double res;
  int exceptions;
  long double xlast;

  assert(pin < 1.0);
  assert(pin > 0.0);  // In fact this should be >= 1/k
  assert(params != NULL);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  // Note, this is looking for a root of the generating function (equation 7.6) in Feller Vol. 1, Chapter XIII, section 7.
  // In general, see this section for a treatment, but in particular equation 7.11.
  N = params[0];
  assert(params[1] <= UINT32_MAX);
  assert(N > 1);

  r = (uint32_t)(params[1]);
  if (r > N + 1) return 0.0;
  assert(r > 1);

  p = (long double)pin;
  // We now know that 0 < p < 1
  // We now know that 0 < q < 1
  q = (1.0L - p);

  x = 1.0L;
  xlast = 0.0L;
  // Calculate x
  // We know that
  // * x is in the interval [1,1/p] which is a subset of [1,k]
  // * the sequence is monotonic up (so x >= xlast).
  // As such we don't need much fanciness for looking for "equality"
  for (j = 0; (j < 65) && ((x - xlast) > LDBL_EPSILON * x); j++) {
    // x=x_j presently
    xlast = x;
    x = 1.0L + q * integerPowl(p * x, r) * x;
    // We expect this function to be monotonic up.
    assert(x >= xlast);
    // We expect this sequence to be bounded above by 1/p.
    assert(x * p <= 1.0L);
    // x=x_(j+1) presently
  }

  // x=x_j
  if (configVerbose > 4) fprintf(stderr, "Iterated %zu times to find a root.\n", j);

  if (configVerbose > 4) fprintf(stderr, "Prediction Estimate: x = %.21Lg\n", x);
  res = logl(1.0L - p * x) - logl(q * (1.0L - ((long double)r) * (x - 1.0L))) - ((N + 1.0L) * logl(x));

  if (configVerbose > 4) fprintf(stderr, "Prediction Estimate: res = %.21Lg\n", res);

  if (fetestexcept(FE_UNDERFLOW) != 0) {
    if (configVerbose > 3) fprintf(stderr, "Prediction Estimate: Underflow encountered in predictionEstFct. Ignoring it.\n");
    feclearexcept(FE_UNDERFLOW);
  }

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW);
  if (exceptions != 0) {
    fprintf(stderr, "Math error in Prediction Estimate Function: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  return (double)res;
}

double calcPglobalBound(double Pglobal, size_t N) {
  double PglobalBound;

  assert(Pglobal >= 0.0);
  assert(Pglobal <= 1.0);
  assert(N > 1);

  if (Pglobal > 0.0) {
    PglobalBound = Pglobal + ZALPHA * sqrt(Pglobal * (1.0 - Pglobal) / ((double)N - 1.0));
    if (PglobalBound > 1.0) {
      PglobalBound = 1.0;
    }
  } else {
    PglobalBound = 1.0 - pow(0.01, 1.0 / ((double)N));
  }

  assert(PglobalBound >= 0.0);
  assert(PglobalBound <= 1.0);
  assert(fetestexcept(FE_UNDERFLOW) == 0);

  return PglobalBound;
}

double calcPrun(double Pglobal, size_t N, size_t r) {
  size_t params[2];
  double pRun;

  params[0] = N;
  params[1] = r;

  // For gathering statistical data on the target value that should be selected.
  if (Pglobal < 1.0) {
    pRun = exp(predictionEstFct(Pglobal, params));
  } else {
    pRun = 1.0;
  }

  if (fetestexcept(FE_UNDERFLOW) != 0) {
    if (configVerbose > 3) fprintf(stderr, "Prediction Estimate: Underflow encountered in predictionEstFct. Ignoring it.\n");
    feclearexcept(FE_UNDERFLOW);
  }

  return pRun;
}

double calcPlocal(size_t N, size_t r, size_t k, double runningMax, size_t rounds, bool noSkip) {
  size_t params[2];
  double Plocal = -1.0;

  assert(rounds > 0);
  assert(k > 0);

  params[0] = N;
  params[1] = r;

  if (noSkip) {
    Plocal = monotonicBinarySearch(predictionEstFct, 1.0 / ((double)k), 1.0, log(0.99) / ((double)rounds), params, true);
  } else {
    // The function is monotonic down in Plocal. The final runningMax is max(PglobalBound,Plocal,1/k).
    // At this point runningMax = max(PglobalBound,1/k), so we don't care if Plocal <= runningMax (this can't affect the result)
    if (((runningMax < 1.0) && (predictionEstFct(runningMax, params) > (log(0.99) / ((double)rounds))))) {
      Plocal = monotonicBinarySearch(predictionEstFct, runningMax, 1.0, log(0.99) / ((double)rounds), params, true);
    }
  }

  if (fetestexcept(FE_UNDERFLOW) != 0) {
    if (configVerbose > 3) fprintf(stderr, "Prediction Estimate: (Expected) Underflow encountered in monotonicBinarySearch. Ignoring it.\n");
    feclearexcept(FE_UNDERFLOW);
  }

  return Plocal;
}

static double predictionEstimateResult(size_t correctCount, size_t N, size_t r, size_t k, struct predictorResult *result) {
  int exceptions;
  double runningMax;

  assert(N > 1);
  assert(k > 1);
  assert(result != NULL);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  result->C = correctCount;
  result->r = r;
  result->N = N;
  result->k = k;

  // This is the minimum probability (corresponding to the maximum entropy assessment)
  runningMax = 1.0 / ((double)k);

  // As Pglobal increases, the assessed min entropy decreases, so we want to determine the Pglobal associated with the 99% upper confidence bound.
  result->Pglobal = (double)correctCount / ((double)N);

  result->PglobalBound = calcPglobalBound(result->Pglobal, N);

  if (result->PglobalBound > runningMax) runningMax = result->PglobalBound;

  // For gathering statistical data on the target value that should be selected.
  result->Prun = calcPrun(result->Pglobal, N, r);

  // Get pLocal (if necessary)
  result->Plocal = calcPlocal(N, r, k, runningMax, 1, configBootstrapParams);
  if (result->Plocal > runningMax) {
    runningMax = result->Plocal;
  }

  result->entropy = -log2(runningMax);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions == FE_UNDERFLOW) {
    fprintf(stderr, "Prediction Estimate: Found an Underflow (result: %.17g ignored and set to 0.0)\n", result->entropy);
    feclearexcept(FE_UNDERFLOW);
    assert(result->entropy < RELEPSILON);
    result->entropy = 0.0;
  } else if (exceptions != 0) {
    fprintf(stderr, "Prediction Estimate Result: Math error: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  result->done = true;

  return result->entropy;
}

/* MultiMCW prediction estimator (6.3.7)*/
double multiMCWPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result) {
  struct multiMCWPredictorState *predictor[4] = {NULL};
  size_t winner;
  size_t curRunOfCorrects;
  size_t maxRunOfCorrects;
  size_t correctCount;
  size_t j, i;
  statData_t fallingOff;

  if (L <= 4095) {
    fprintf(stderr, "MultiMCW only defined for data samples larger than 4095 samples.\n");
    return -1.0;
  }

  // Initialize the predictors
  predictor[0] = initMCWPredictor(S, L, k, 63);
  predictor[1] = initMCWPredictor(S, L, k, 255);
  predictor[2] = initMCWPredictor(S, L, k, 1023);
  predictor[3] = initMCWPredictor(S, L, k, 4095);

  winner = 0;
  maxRunOfCorrects = 0;
  curRunOfCorrects = 0;
  correctCount = 0;
  for (i = 63; i < L; i++) {
    if (S[i] == predictor[winner]->prediction) {
      correctCount++;
      curRunOfCorrects++;
    } else {
      curRunOfCorrects = 0;
    }

    if (curRunOfCorrects > maxRunOfCorrects) {
      maxRunOfCorrects = curRunOfCorrects;
    }

    for (j = 0; j < 4; j++) {
      if ((predictor[j] != NULL) && (predictor[j]->windowSize <= i)) {
        if (predictor[j]->prediction == S[i]) {
          predictor[j]->correctPredictions++;
          if (predictor[j]->correctPredictions >= predictor[winner]->correctPredictions) {
            winner = j;
          }
        }
      }
    }

    for (j = 0; j < 4; j++) {
      if ((predictor[j] != NULL) && (predictor[j]->windowSize <= i)) {
        // Now update the state to reflect the new value.
        fallingOff = S[i - predictor[j]->windowSize];
        predictor[j]->counts[fallingOff]--;
        predictor[j]->counts[S[i]]++;
        predictor[j]->symbolLastSeen[S[i]] = i;
        if (predictor[j]->prediction == fallingOff) {
          updateMultiMCWPrediction(predictor[j]);
        } else {
          if (predictor[j]->counts[predictor[j]->prediction] <= predictor[j]->counts[S[i]]) {
            predictor[j]->prediction = S[i];
          }
        }
      }
    }
  }

  for (j = 0; j < 4; j++) {
    delMultiMCWPredictor(predictor[j]);
    predictor[j] = NULL;
  }

  return (predictionEstimateResult(correctCount, L - 63, maxRunOfCorrects + 1, k, result));
}

// It is important that LAGD be a power of 2 (some of the fanciness would otherwise break)
#define LAGD 128LU
#define LAGMASK (LAGD - 1)

/*Convention:
 * if start==end, the buffer is empty. Start and end values are not ANDed, so range from 0...255
 * This extended range allows for use of all the entries of the buffer
 * See https://www.snellman.net/blog/archive/2016-12-13-ring-buffers/
 * The actual data locations range 0...127
 * start&LAGMASK is the index of the oldest data
 * end&LAGMASK is the index where the *next* data goes.
 */

struct lagBuf {
  uint8_t start;
  uint8_t end;
  size_t buf[LAGD];
};

/* Lag prediction estimate (6.3.8)
 * This is a somewhat counter-intuitive approach to this test; the original idea for this approach is due
 * to David Oksner. The straight forward way is simply to check j symbols back for each case (where j runs
 * 1 to 128). It ends up that this is bizarrely slow.
 * This approach is to keep a list of offsets where we encountered each symbol (in a ring buffer,
 * which can store at most D (128) prior elements.
 * For this, one needs only check and update the current symbol's ring buffer, and we only need to spend
 * time looking at values that correspond to counters that must be updated.
 */
double lagPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result) {
  size_t scoreboard[LAGD] = {0};
  size_t winner = 0;
  size_t curRunOfCorrects = 0;
  size_t maxRunOfCorrects = 0;
  size_t correctCount = 0;
  struct lagBuf *ringBuffers;
  size_t highScore = 0;

  assert(S != NULL);
  assert(result != NULL);
  assert(L > 2);
  assert(k >= 2);

  if ((ringBuffers = malloc(k * sizeof(struct lagBuf))) == NULL) {
    perror("Can't allocate ring buffers for lag prediction");
    exit(EX_OSERR);
  }

  // Flag all the rings as empty
  for (size_t j = 0; j < k; j++) {
    ringBuffers[j].start = 0;
    ringBuffers[j].end = 0;
  }

  // Account for the very first symbol (there isn't a guess for this one)
  ringBuffers[S[0]].buf[0] = 0;
  ringBuffers[S[0]].start = 0;
  ringBuffers[S[0]].end = 1;

  // The rest of the values yield a prediction
  for (size_t i = 1; i < L; i++) {
    const statData_t curSymbol = S[i];
    struct lagBuf *const curRingBuffer = &(ringBuffers[curSymbol]);  // The pointer itself is a constant (not the thing it points to)

    // Check the prediction first
    if (curSymbol == S[i - winner - 1]) {
      correctCount++;
      curRunOfCorrects++;
      if (curRunOfCorrects > maxRunOfCorrects) {
        maxRunOfCorrects = curRunOfCorrects;
      }
    } else {
      curRunOfCorrects = 0;
    }

    // Update counters
    if (curRingBuffer->start != curRingBuffer->end) {
      uint8_t counterIndex = curRingBuffer->end;
      const size_t cutoff = likely(i >= LAGD) ? (i - LAGD) : 0;  // Cutoff is the oldest stream index that should be present in the buffer

      do {
        counterIndex--;
        if ((curRingBuffer->buf[counterIndex & LAGMASK]) >= cutoff) {
          size_t curScore;
          size_t curOffset;
          curOffset = i - (curRingBuffer->buf[counterIndex & LAGMASK]) - 1;
          assert(curOffset < LAGD);
          curScore = ++scoreboard[curOffset];

          if (curScore >= highScore) {
            winner = curOffset;
            highScore = curScore;
          }
        } else {
          // The correct start was the prior symbol (which is the next symbol in the buffer)
          curRingBuffer->start = (uint8_t)(counterIndex + 1U);
          break;
        }
      } while (counterIndex != curRingBuffer->start);
    }

    // Add the new symbol
    // Are we already full? If so, advance the start index.
    if ((uint8_t)(curRingBuffer->end - curRingBuffer->start) == LAGD) curRingBuffer->start++;
    // Add the current offset and adjust the end index
    curRingBuffer->buf[(curRingBuffer->end) & LAGMASK] = i;
    curRingBuffer->end++;
    assert((uint8_t)(curRingBuffer->end - curRingBuffer->start) <= LAGD);
  }

  free(ringBuffers);

  return (predictionEstimateResult(correctCount, L - 1, maxRunOfCorrects + 1, k, result));
}

/* There are effectively two different implementations of the MultiMMC (6.3.9) and LZ78Y (6.3.10) predictors here.
 * One for binary data (which avoids any sort of expensive data structure),
 * and one that is intended to be used for non-binary data.
 * The main implementation for the non-binary case is based on a k-ary tree.
 */

/*The indexing of the MultiMMC and LZ78Y is vacuously different. Set I=i-d for the MultiMMC and I=i-j for the LZ78Y
  The pattern in the MultiMMC and LZ78Y tests is to
  (a) look up d-string starting at index s_(I-1) in the data structure in order to initialize them using incrementDict(), and
  (b) then conduct a prediction using the d-string at index s_(I) using predictDict().
  Note that the look up in (b) is the same location that we're going to be looking up in (a) when i increments for the next loop
  (so the values looked up in (b) are the same values looked up in the next loop iteration's step (a).
  By caching the result in (b) we can generally avoid a separate look up in (a)
  (which roughly halves the number of structure look ups!)
  The NIST approach to this is wonderful; they note that (with the correct setup) you can interleave the prediction
  and the increment of the same size. We ultimately adopt this for all the main implementations.
 */

// The idea here is that we've given an array of pointers (binaryDict).
// We are trying to produce the address of the length-2 array associated with the length-d prefix "b".
// array The dth index is d-1, so we first find the start of the address space (binaryDict[(d)-1])
// We take the least significant d bits from "b": this is the expression "(b) & ((1U << (d)) - 1)"
// We then multiply this by 2 (as each pattern is associated with a length-2 array) by left shifting by 1.
#define BINARYDICTLOC(d, b) (binaryDict[(d)-1] + (((b) & ((1U << (d)) - 1)) << 1))

// First, the binary implementations (which are dramatically faster, due to the absence of any complicated data structure)
/* This implementation of the MultiMMC test is a based on NIST's really cleaver implementation,
 * which interleaves the predictions and updates. This makes optimization much easier.
 * It's opaque why it works correctly (in particular, the first few symbols are added in a different
 * order than in the reference implementation), but once the initialization is performed, the rest of the
 * operations are done in the correct order.
 * The general observations that explains why this approach works are that
 * 1) each prediction that could succeed (i.e., ignoring some of the early predictions that must fail due
 *    to lack of strings of the queried length) must occur only after all the correct (x,y) tuples for that
 *    length have been processed. One is free to reorder otherwise.
 * 2) If there is a distinct string of length n, then this induces corresponding unique strings of all
 *    lengths greater than n. We track all string lengths independently (thus conceptually, we
 *    could run out of a short-string length prior to a long string length, thus erroneously not add
 *    some long string to the dictionary after no longer looking for a string to the dictionary when
 *    we should have), this can't happen in practice because we add strings from shortest to longest.
 */
#define MULTIMMCD 16U
#define MULTIMMCMAXENT 100000U
// Note that this is structured after Aaron Kaufer's implementation in the NIST tool; this formulation allows for
// interleaving the update and prediction for the same symbol length
static double binaryMultiMMCPredictionEstimate(const statData_t *S, size_t L, struct predictorResult *result) {
  size_t scoreboard[MULTIMMCD] = {0};
  size_t *binaryDict[MULTIMMCD] = {NULL};
  size_t winner = 0;
  size_t curWinner;
  size_t curRunOfCorrects = 0;
  size_t maxRunOfCorrects = 0;
  size_t correctCount = 0;
  size_t j, d, i;
  uint32_t curPattern = 0;
  size_t dictElems[MULTIMMCD] = {0};

  assert(L > 3);
  assert(MULTIMMCD < 32);  // MULTIMMCD < 32 to make the bit shifts well defined

  // Initialize the dictionary memory
  for (j = 0; j < MULTIMMCD; j++) {
    // For a length m prefix, we need 2^m sets of length 2 arrays.
    // Here, j+1 is the length of the prefix, so we need 2^(j+1) prefixes, or 2*2^(j+1) = 2^(j+2) storage total.
    // Note: 2^(j+2) = 1<<(j+2).
    // calloc sets the values to all 0.
    if ((binaryDict[j] = calloc(1U << (j + 2), sizeof(size_t))) == NULL) {
      perror("Can't allocate array binary dictionary");
      exit(EX_OSERR);
    }
  }

  // initialize MMC counts
  for (d = 0; d < MULTIMMCD; d++) {
    curPattern = ((curPattern << 1) | (S[d] & 1));

    // This is necessarily the first symbol of this length
    (BINARYDICTLOC(d + 1, curPattern))[S[d + 1] & 1] = 1;
    dictElems[d] = 1;
  }

  // In C, arrays are 0 indexed.
  // i is the index of the new symbol to be predicted
  for (i = 2; i < L; i++) {
    bool found_x = false;
    curWinner = winner;
    curPattern = 0;

    // d+1 is the number of symbols used by the predictor
    for (d = 0; (d < MULTIMMCD) && (d <= i - 2); d++) {
      uint8_t curPrediction = 2;
      size_t curCount;
      size_t *binaryDictEntry;

      // Add S[i-d-1] to the start of curPattern
      curPattern = (curPattern | (((uint32_t)(S[i - d - 1] & 1)) << d));
      // curPattern should contain the d-tuple (S[i-d-1] ... S[i-1])

      binaryDictEntry = BINARYDICTLOC(d + 1, curPattern);

      // check if the prefix x has been previously seen. If the prefix x has not occurred,
      // then do not make a prediction for current d and larger d's
      // as well, since it will not occur for them either. In other words,
      // prediction is NULL, so do not update the scoreboard.
      // Note that found_x is meaningless on the first round, but for that round d==0.
      // All future rounds use a meaningful found_x
      if ((d == 0) || found_x) {
        // For the prediction, curPrediction is the max across all pairs (there are only 2 symbols here!)
        if ((binaryDictEntry[0] > binaryDictEntry[1])) {
          curPrediction = 0;
          curCount = binaryDictEntry[0];
        } else {
          curPrediction = 1;
          curCount = binaryDictEntry[1];
        }

        if (curCount == 0)
          found_x = false;
        else
          found_x = true;
      }

      if (found_x) {
        // x is present as a prefix.
        // Check to see if the current prediction is correct.
        if (curPrediction == S[i]) {
          // prediction is correct, update scoreboard and (the next round's) winner
          scoreboard[d]++;
          if (scoreboard[d] >= scoreboard[winner]) winner = d;

          // If the best predictor was previously d, increment the relevant counters
          if (d == curWinner) {
            correctCount++;
            curRunOfCorrects++;
            if (curRunOfCorrects > maxRunOfCorrects) maxRunOfCorrects = curRunOfCorrects;
          }
        } else if (d == curWinner) {
          // This prediction was wrong;
          // If the best predictor was previously d, zero the run length counter
          curRunOfCorrects = 0;
        }

        // Now check to see in (x,y) needs to be counted or (x,y) added to the dictionary
        if (binaryDictEntry[S[i] & 1] != 0) {
          // The (x,y) tuple has already been encountered.
          // Increment the existing entry
          binaryDictEntry[S[i] & 1]++;
        } else if (dictElems[d] < MULTIMMCMAXENT) {
          // The x prefix has been encountered, but not (x,y)
          // We're allowed to make a new entry. Do so.
          binaryDictEntry[S[i] & 1] = 1;
          dictElems[d]++;
        }
      } else if (dictElems[d] < MULTIMMCMAXENT) {
        // We didn't find the x prefix, so (x,y) surely can't have occurred.
        // We're allowed to make a new entry. Do so.
        binaryDictEntry[S[i] & 1] = 1;
        dictElems[d]++;
      }
    }
  }

  for (j = 0; j < MULTIMMCD; j++) {
    free(binaryDict[j]);
    binaryDict[j] = NULL;
  }

  return (predictionEstimateResult(correctCount, L - 2, maxRunOfCorrects + 1, 2, result));
}

#define LZ78YB 16U
#define LZ78YMAXDICT 65536U
static double binaryLZ78YPredictionEstimate(const statData_t *S, size_t L, struct predictorResult *result) {
  size_t *binaryDict[LZ78YB] = {NULL};
  size_t curRunOfCorrects = 0;
  size_t maxRunOfCorrects = 0;
  size_t correctCount = 0;
  size_t i, j;
  uint32_t curPattern = 0;
  size_t dictElems = 0;

  assert(L > LZ78YB);
  assert(L - LZ78YB > 2);
  assert(LZ78YB < 32);  // LZ78YB < 32 to make the bit shifts well defined

  // Initialize the dictionary memory
  for (j = 0; j < LZ78YB; j++) {
    // For a length m prefix, we need 2^m sets of length 2 arrays.
    // Here, j+1 is the length of the prefix, so we need 2^(j+1) prefixes, or 2*2^(j+1) = 2^(j+2) storage total.
    // Note: 2^(j+2) = 1<<(j+2).
    // calloc sets the values to all 0.
    if ((binaryDict[j] = calloc(1U << (j + 2), sizeof(size_t))) == NULL) {
      perror("Can't allocate array binary dictionary");
      exit(EX_OSERR);
    }
  }

  // initialize LZ78Y counts with {(S[15]), S[16]}, {(S[14], S[15]), S[16]}, ..., {(S[0]), S[1], ..., S[15]), S[16]},
  for (j = 0; j < LZ78YB; j++) {
    curPattern = curPattern | (((uint32_t)(S[LZ78YB - j - 1] & 1)) << j);

    // This is necessarily the first symbol of this length
    (BINARYDICTLOC(j + 1, curPattern))[S[LZ78YB] & 1] = 1;
    dictElems++;
  }

  // In C, arrays are 0 indexed.
  // i is the index of the new symbol to be predicted
  for (i = LZ78YB + 1; i < L; i++) {
    bool found_x;
    bool havePrediction = false;
    statData_t roundPrediction = 2;
    statData_t curPrediction = 2;
    size_t maxCount = 0;

    // Build up the longest (LZ78YB-length) string we're going to need
    curPattern = 0;
    for (j = 0; j < LZ78YB; j++) {
      curPattern = curPattern | (((uint32_t)(S[i - 1 - j] & 1)) << j);
    }

    for (j = LZ78YB; j > 0; j--) {
      size_t curCount;
      size_t *binaryDictEntry;

      // curPattern starts off as long as possible. We then clear bits at the end
      // as we shorten curPattern
      curPattern = curPattern & ((1U << j) - 1);
      // curPattern should contain the j-tuple (S[i-j] ... S[i-1])

      binaryDictEntry = BINARYDICTLOC(j, curPattern);

      // check if x has been previously seen.
      // For the prediction, roundPrediction is the max across all pairs (there are only 2 symbols here!)
      if ((binaryDictEntry[0] > binaryDictEntry[1])) {
        roundPrediction = 0;
        curCount = binaryDictEntry[0];
      } else {
        roundPrediction = 1;
        curCount = binaryDictEntry[1];
      }

      if (curCount == 0) {
        found_x = false;
      } else {
        found_x = true;
      }

      if (found_x) {
        // x is present in the dictionary as a prefix.
        if (curCount > maxCount) {
          maxCount = curCount;
          havePrediction = true;
          curPrediction = roundPrediction;
        }

        binaryDictEntry[S[i] & 1]++;
      } else if (dictElems < LZ78YMAXDICT) {
        // We didn't find the x prefix, so (x,y) surely can't have occurred.
        // We're allowed to make a new entry. Do so.
        binaryDictEntry[S[i] & 1] = 1;
        dictElems++;
      }
    }

    // Check to see if the current prediction is correct.
    if (havePrediction && (curPrediction == S[i])) {
      correctCount++;
      curRunOfCorrects++;
      if (curRunOfCorrects > maxRunOfCorrects) maxRunOfCorrects = curRunOfCorrects;
    } else {
      curRunOfCorrects = 0;
    }
  }

  for (j = 0; j < LZ78YB; j++) {
    free(binaryDict[j]);
    binaryDict[j] = NULL;
  }

  return (predictionEstimateResult(correctCount, L - LZ78YB - 1, maxRunOfCorrects + 1, 2, result));
}

// The k-ary tree implementations
/* This implementation of the MultiMMC test is a based on NIST's really cleaver implementation,
 * which interleaves the predictions and updates. This makes optimization much easier.
 * It's opaque why it works correctly (in particular, the first few symbols are added in a different
 * order than in the reference implementation), but once the initialization is performed, the rest of the
 * operations are done in the correct order.
 * The general observations that explains why this approach works are that
 * 1) each prediction that could succeed (i.e., ignoring some of the early predictions that must fail due
 *    to lack of strings of the queried length) must occur only after all the correct (x,y) tuples for that
 *    length have been processed. One is free to reorder otherwise.
 * 2) If there is a distinct string of length n, then this induces corresponding unique strings of all
 *    lengths greater than n. We track all string lengths independently (thus conceptually, we
 *    could run out of a short-string length prior to a long string length, thus erroneously not add
 *    some long string to the dictionary after no longer looking for a string to the dictionary when
 *    we should have), this can't happen in practice because we add strings from shortest to longest.
 */
// Note that this is structured after Aaron Kaufer's implementation in the NIST tool; this formulation allows for
// interleaving the update and prediction for the same symbol length
double treeMultiMMCPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result) {
  size_t scoreboard[MULTIMMCD] = {0};
  size_t winner = 0;
  size_t curWinner = 0;
  size_t curRunOfCorrects = 0;
  size_t maxRunOfCorrects = 0;
  size_t correctCount = 0;
  size_t j, d, i;
  size_t dictElems[MULTIMMCD] = {0};

  struct dictionaryPage *dictHead = NULL;

  size_t modulusCount[MODULUSCOUNT] = {0};
  size_t occupiedCount[MODULUSCOUNT] = {0};
  size_t dictPageCount = 0;
  struct memSegment *mempools[MODULUSCOUNT + 1] = {NULL};
  size_t poolMem = 0;
  size_t curPoolMem = 0;

  assert(L > 3);
  assert(MULTIMMCD < 32);

  if (k == 2) return binaryMultiMMCPredictionEstimate(S, L, result);
  assert(k > 2);

  // setup the memory pools
  for (j = 0; j < MODULUSCOUNT - 1; j++) {
    if ((size_t)hashModulus[j] < k) {
      assert(hashModulus[j] > 0);
      if (configVerbose > 4) fprintf(stderr, "Pool %zu: Getting pool for modulus %d\n", j, hashModulus[j]);
      mempools[j] = initPool(((size_t)hashModulus[j]) * sizeof(struct dictionaryEntry), 512);
    }
  }
  if (configVerbose > 4) fprintf(stderr, "Pool %zu: Getting pool for modulus %zu\n", (size_t)MODULUSCOUNT - 1, k);
  mempools[MODULUSCOUNT - 1] = initPool(k * sizeof(struct dictionaryEntry), 512);
  if (configVerbose > 4) fprintf(stderr, "Pool %zu: Getting pool for dictionary pages\n", (size_t)MODULUSCOUNT);
  mempools[MODULUSCOUNT] = initPool(sizeof(struct dictionaryPage), MULTIMMCMAXENT * MULTIMMCD);

  // Initialize the head of the dictionary page structure
  dictHead = newDictionaryPage(mempools);

  // initialize MMC counts
  for (d = 0; d < MULTIMMCD; d++) {
    // This is necessarily the first symbol of this length
    treeIncrementDict(S, d + 1, dictHead, k, true, true, NULL, mempools);
    dictElems[d] = 1;
  }

  // In C, arrays are 0 indexed.
  // i is the index of the new symbol to be predicted
  for (i = 2; i < L; i++) {
    bool found_x = false;
    curWinner = winner;

    // d+1 is the number of symbols used by the predictor
    for (d = 0; (d < MULTIMMCD) && (d <= i - 2); d++) {
      statData_t curPrediction = 0;
      size_t curCount;
      struct dictionaryPage *locCache = NULL;

      // check if x has been previously seen as a prefix. If the prefix x has not occurred,
      // then do not make a prediction for current d and larger d's
      // as well, since it will not occur for them either. In other words,
      // prediction is NULL, so do not update the scoreboard.
      // Note that found_x is uninitialized on the first round, but for that round d==0.
      if ((d == 0) || found_x) {
        // Get the prediction
        // predict S[i] by using the prior d+1 symbols and the current state
        // We need the d-tuple prior to S[i], that is (S[i-d-1], ..., S[i-1])

        // This populates the locCache for the increment
        curCount = treePredictDict(S + i - d - 1, d + 1, &curPrediction, dictHead, &locCache);

        if (curCount == 0)
          found_x = false;
        else
          found_x = true;
      }

      if (found_x) {
        bool makeBranches;

        // x is present as a prefix.
        // Check to see if the current prediction is correct.
        if (curPrediction == S[i]) {
          // prediction is correct, update scoreboard and (the next round's) winner
          scoreboard[d]++;
          if (scoreboard[d] >= scoreboard[winner]) winner = d;

          // If the best predictor was previously d, increment the relevant counters
          if (d == curWinner) {
            correctCount++;
            curRunOfCorrects++;
            if (curRunOfCorrects > maxRunOfCorrects) maxRunOfCorrects = curRunOfCorrects;
          }
        } else if (d == curWinner) {
          // This prediction was wrong;
          // If the best predictor was previously d, zero the run length counter
          curRunOfCorrects = 0;
        }

        // Now check to see in (x,y) needs to be counted or (x,y) added to the dictionary
        makeBranches = dictElems[d] < MULTIMMCMAXENT;
        if (treeIncrementDict(S + i - d - 1, d + 1, dictHead, k, makeBranches, true, locCache, mempools) && makeBranches) {
          dictElems[d]++;
        }
      } else if (dictElems[d] < MULTIMMCMAXENT) {
        // We didn't find the x prefix, so (x,y) surely can't have occurred.
        // We're allowed to make a new entry. Do so.
        treeIncrementDict(S + i - d - 1, d + 1, dictHead, k, true, true, NULL, mempools);
        dictElems[d]++;
      }
    }
  }

  dictPageCount = delDictionary(dictHead, k, modulusCount, occupiedCount, mempools);
  if (configVerbose > 3) {
    size_t dictEntryCount = 0;
    for (j = 0; j < MULTIMMCD; j++)
      if (configVerbose > 3) fprintf(stderr, "Dictionary[%zu]: has %zu entries\n", j, dictElems[j]);
    fprintf(stderr, "Total dictionary pages: %zu (%.5g MB).\n", dictPageCount, ((double)(dictPageCount * sizeof(struct dictionaryPage))) / 1048576.0);
    fprintf(stderr, "Hash table size distribution:");
    for (j = 0; j < MODULUSCOUNT; j++) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      fprintf(stderr, " %zu: %zu (%zu Bytes)", curMod, modulusCount[j], curMod * modulusCount[j] * sizeof(struct dictionaryEntry));
      dictEntryCount += curMod * modulusCount[j];
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Dictionary entries: %zu (%.5g MB).\n", dictEntryCount, ((double)(dictEntryCount * sizeof(struct dictionaryEntry))) / 1048576.0);
    fprintf(stderr, "Hash table average occupancy rate:");
    for (j = 0; j < MODULUSCOUNT; j++) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      if (modulusCount[j] != 0) {
        double proportionUsed = (double)occupiedCount[j] / (((double)curMod) * ((double)modulusCount[j]));
        fprintf(stderr, " %zu: %.5g (%.5g MB wasted)", curMod, proportionUsed, (1.0 - proportionUsed) * ((double)(sizeof(struct dictionaryEntry) * curMod)) * ((double)modulusCount[j]) / 1048576.0);
      }
    }
    fprintf(stderr, "\n");
  }

  // tare down the memory pools
  poolMem = 0;
  for (j = 0; j < MODULUSCOUNT; j++) {
    if (mempools[j] != NULL) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      size_t bCount;
      curPoolMem = delPool(mempools[j]);
      bCount = curPoolMem / (curMod * sizeof(struct dictionaryEntry));
      poolMem += curPoolMem;
      if (configVerbose > 3) fprintf(stderr, "Block allocator %zu takes %zu bytes (%zu of %zu used)\n", j, curPoolMem, modulusCount[j], bCount);
    }
  }
  curPoolMem = delPool(mempools[MODULUSCOUNT]);
  poolMem += curPoolMem;
  if (configVerbose > 3) fprintf(stderr, "Block allocator %zu takes %zu bytes (%zu of %zu used)\n", j, curPoolMem, dictPageCount, curPoolMem / sizeof(struct dictionaryPage));

  if (configVerbose > 3) fprintf(stderr, "Total memory consumed by block allocator: %zu\n", poolMem);

  return (predictionEstimateResult(correctCount, L - 2, maxRunOfCorrects + 1, k, result));
}

double treeLZ78YPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result) {
  size_t curRunOfCorrects = 0;
  size_t maxRunOfCorrects = 0;
  size_t correctCount = 0;
  size_t i, j;
  size_t dictElems = 0;

  struct dictionaryPage *dictHead = NULL;

  size_t modulusCount[MODULUSCOUNT] = {0};
  size_t occupiedCount[MODULUSCOUNT] = {0};
  size_t dictPageCount = 0;
  struct memSegment *mempools[MODULUSCOUNT + 1] = {NULL};
  size_t poolMem = 0;
  size_t curPoolMem = 0;

  assert(L > LZ78YB);
  assert(L - LZ78YB > 2);
  assert(LZ78YB < 32);
  assert(k > 1);

  if (k == 2) return binaryLZ78YPredictionEstimate(S, L, result);

  // setup the memory pools
  for (j = 0; j < MODULUSCOUNT - 1; j++) {
    if ((size_t)hashModulus[j] < k) {
      assert(hashModulus[j] > 0);
      if (configVerbose > 3) fprintf(stderr, "Pool %zu: Getting pool for modulus %d\n", j, hashModulus[j]);
      mempools[j] = initPool(((size_t)hashModulus[j]) * sizeof(struct dictionaryEntry), 512);
    }
  }

  if (configVerbose > 3) fprintf(stderr, "Pool %zu: Getting pool for modulus %zu\n", (size_t)MODULUSCOUNT - 1, k);
  mempools[MODULUSCOUNT - 1] = initPool(k * sizeof(struct dictionaryEntry), 512);
  if (configVerbose > 3) fprintf(stderr, "Pool %zu: Getting pool for dictionary pages\n", (size_t)MODULUSCOUNT);
  mempools[MODULUSCOUNT] = initPool(sizeof(struct dictionaryPage), LZ78YMAXDICT);

  dictHead = newDictionaryPage(mempools);

  // initialize LZ78Y counts with {(S[15]), S[16]}, {(S[14], S[15]), S[16]}, ..., {(S[0]), S[1], ..., S[15]), S[16]}
  for (j = 1; j <= LZ78YB; j++) {
    bool entryCreated;
    // This is necessarily the first symbol of this length
    entryCreated = treeIncrementDict(S + LZ78YB - j, j, dictHead, k, true, false, NULL, mempools);
    assert(entryCreated);
    dictElems++;
  }

  // In C, arrays are 0 indexed.
  // i is the index of the new symbol to be predicted
  for (i = LZ78YB + 1; i < L; i++) {
    bool found_x;
    bool havePrediction = false;
    statData_t curPrediction = 0;
    size_t maxCount = 0;

    for (j = LZ78YB; j > 0; j--) {
      size_t curCount;
      struct dictionaryPage *locCache = NULL;
      statData_t roundPrediction = 0;

      // check if x has been previously seen.
      // For the prediction, roundPrediction is the max across all pairs
      // The prefix string should contain the j-tuple (S[i-j] ... S[i-1])
      curCount = treePredictDict(S + i - j, j, &roundPrediction, dictHead, &locCache);

      if (curCount == 0) {
        found_x = false;
      } else {
        found_x = true;
      }

      if (found_x) {
        bool entryCreated;

        // x is present in the dictionary as a prefix.
        if (curCount > maxCount) {
          maxCount = curCount;
          havePrediction = true;
          curPrediction = roundPrediction;
        }

        // We found the prefix, and this predictor always creates new postfixes
        assert(locCache != NULL);
        entryCreated = treeIncrementDict(S + i - j, j, dictHead, k, true, false, locCache, mempools);
        assert(!entryCreated);
      } else if (dictElems < LZ78YMAXDICT) {
        bool entryCreated;

        // We didn't find the x prefix, so (x,y) surely can't have occurred.
        // We're allowed to make a new entry. Do so.
        entryCreated = treeIncrementDict(S + i - j, j, dictHead, k, true, false, locCache, mempools);
        assert(entryCreated);
        dictElems++;
      }
    }

    // Check to see if the current prediction is correct.
    if (havePrediction && (curPrediction == S[i])) {
      correctCount++;
      curRunOfCorrects++;
      if (curRunOfCorrects > maxRunOfCorrects) maxRunOfCorrects = curRunOfCorrects;
    } else {
      curRunOfCorrects = 0;
    }
  }

  dictPageCount = delDictionary(dictHead, k, modulusCount, occupiedCount, mempools);
  if (configVerbose > 3) {
    size_t dictEntryCount = 0;
    if (configVerbose > 3) fprintf(stderr, "Dictionary: has %zu entries\n", dictElems);
    fprintf(stderr, "Total dictionary pages: %zu (%.5g MB)\n", dictPageCount, ((double)(dictPageCount * sizeof(struct dictionaryPage))) / 1048576.0);
    fprintf(stderr, "Hash table size distribution:");
    for (j = 0; j < MODULUSCOUNT; j++) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      fprintf(stderr, " %zu: %zu (%zu Bytes)", curMod, modulusCount[j], curMod * modulusCount[j] * sizeof(struct dictionaryEntry));
      dictEntryCount += curMod * modulusCount[j];
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "Dictionary entries: %zu (%.5g MB).\n", dictEntryCount, ((double)(dictEntryCount * sizeof(struct dictionaryEntry))) / 1048576.0);
    fprintf(stderr, "Hash table average occupancy rate:");
    for (j = 0; j < MODULUSCOUNT; j++) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      if (modulusCount[j] != 0) {
        double proportionUsed = (double)occupiedCount[j] / (((double)curMod) * ((double)modulusCount[j]));
        fprintf(stderr, " %zu: %.5g (%.5g MB wasted)", curMod, proportionUsed, (1.0 - proportionUsed) * ((double)(sizeof(struct dictionaryEntry) * curMod)) * ((double)modulusCount[j]) / 1048576.0);
      }
    }
    fprintf(stderr, "\n");
  }

  poolMem = 0;
  // tare down the memory pools
  for (j = 0; j < MODULUSCOUNT; j++) {
    if (mempools[j] != NULL) {
      size_t curMod = (hashModulus[j] == 0) ? k : ((size_t)hashModulus[j]);
      size_t bCount;
      curPoolMem = delPool(mempools[j]);
      bCount = curPoolMem / (curMod * sizeof(struct dictionaryEntry));
      poolMem += curPoolMem;
      if (configVerbose > 3) fprintf(stderr, "Block allocator %zu takes %zu bytes (%zu of %zu used)\n", j, curPoolMem, modulusCount[j], bCount);
    }
  }
  curPoolMem = delPool(mempools[MODULUSCOUNT]);
  poolMem += curPoolMem;
  if (configVerbose > 3) fprintf(stderr, "Block allocator %zu takes %zu bytes (%zu of %zu used)\n", j, curPoolMem, dictPageCount, curPoolMem / sizeof(struct dictionaryPage));

  if (configVerbose > 3) fprintf(stderr, "Total memory consumed by block allocator: %zu\n", poolMem);

  return (predictionEstimateResult(correctCount, L - LZ78YB - 1, maxRunOfCorrects + 1, k, result));
}

static double mindouble(double *list, size_t len, size_t *chosen) {
  double minval;
  size_t i;

  assert(list != NULL);
  if (len == 0) {
    return -1.0;
  }

  minval = *list;
  if (chosen) {
    *chosen = 0;
  }
  list++;

  for (i = 1; i < len; i++) {
    if (*list < minval) {
      minval = *list;
      if (chosen) {
        *chosen = i;
      }
    }
    list++;
  }

  return (minval);
}

/* When "conservative" is true, then this is an corrected implementation of the (now historic) 2016 draft of the Markov Estimate (6.3.3)
 * When "conservative" is false, this is a less conservative version that doesn't make an assumption of the normality of gathered statistics,
 * and doesn't generate confidence intervals based on this assumption
 */

#define DESIRABLE_MAX_EPSILON 0.05
double NSAMarkovEstimate(const statData_t *S, size_t L, size_t k, const char *label, bool conservative, double probCutoff) {
  size_t d = 128;

  size_t *count;
  size_t *oij;

  double *T;
  double *P;
  double *Pp;
  double *h;
  double *tempdoubleptr;

  double chain_minentropy;
  double result;

  statData_t lastsymbol;
  size_t i, j, c;

  const statData_t *curdataptr;
  double curprob;

  int exceptions;

  double alpha = 0.99;
  double epsilon_term = 0.0;
  double epsilon = 0.0;
  double maxEpsilon = -1.0;
  size_t countCutoff;
  bool isStable;
  bool reducedTrailingSymbolCount;

  assert(probCutoff < 1.0);
  assert(probCutoff >= 0.0);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  assert(S != NULL);
  if (L <= 2) {
    fprintf(stderr, "Markov Estimate only defined for data samples larger than 1 sample.\n");
    return -1.0;
  }

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  count = calloc(k, sizeof(size_t));
  oij = calloc(k * k, sizeof(size_t));
  T = malloc(sizeof(double) * k * k);
  P = malloc(sizeof(double) * k);
  Pp = malloc(sizeof(double) * k);
  h = malloc(sizeof(double) * k);

  if (!count || !oij || !T || !P || !Pp || !h) {
    perror("Memory allocation error");
    exit(EX_OSERR);
  }

  for (i = 0; i < k; i++) {
    P[i] = DBL_INFINITY;
  }

  countCutoff = (size_t)floor(probCutoff * (double)L);

  if (configVerbose > 0) {
    fprintf(stderr, "%s NSA Markov Estimate: Symbol cutoff probability is %.17g.\n", label, probCutoff);
    fprintf(stderr, "%s NSA Markov Estimate: Symbol cutoff count is %zu.\n", label, countCutoff);
  }

  /*Initialize oij and counts*/
  lastsymbol = S[0];
  assert((size_t)lastsymbol < k);
  curdataptr = S + 1;
  count[lastsymbol]++;

  for (i = 1; i < L; i++) {
    assert((size_t)*curdataptr < k);
    count[*curdataptr]++;
    oij[((size_t)lastsymbol) * k + (size_t)(*curdataptr)]++;
    lastsymbol = *curdataptr;
    curdataptr++;
  }

  isStable = false;

  // We don't a priori know how many symbols are ultimately going to be excluded. Loop until all the
  // number of transitions from each symbol to a valid symbol stabilizes.
  while (!isStable) {
    size_t validSymbolCount = 0;
    size_t localk = 0;
    bool hasNotIterateInformed = true;

    isStable = true;
    maxEpsilon = -1.0;

    // Which symbols are valid?
    if (countCutoff > 0) {
      for (i = 0; i < k; i++) {
        if (count[i] >= countCutoff) {
          localk++;
          validSymbolCount += count[i];
        }
      }
    } else {
      validSymbolCount = L;
      localk = k;
    }

    if (configVerbose > 0) {
      fprintf(stderr, "%s NSA Markov Estimate: There are %zu sufficiently common distinct symbols in a dataset of %zu samples.\n", label, localk, validSymbolCount);
    }

    assert(validSymbolCount > 0);
    assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

    if (conservative) {
      double alphaT;
      double alpha_exp;
      /*SP800-90B specifies taking the min of the resulting computations. */
      /*alpha_exp = k*k;
      if(d > alpha_exp) alpha_exp = d;
      alpha = pow(0.99, alpha_exp);*/

      /*The Hagerty/Draper suggests that the overall alpha is related to the
      alpha here in a somewhat more complicated way.*/
      /*log base 2 makes no sense (see Hoeffding's Inequality)
       * The k^2 term isn't sufficient, as we have to estimate the initial probabilities as well.
       */

      /*We are really only estimating localk symbols; the others are not suitably common...*/
      alpha_exp = (double)(localk * localk + localk);
      assert(alpha_exp > 0.0);

      /*This doesn't fulfill the hypotheses of C.1
      This also shouldn't ever happen, given the restricted parameters.*/
      // if(d < alpha_exp) alpha_exp = d;

      alphaT = pow(alpha, 1.0 / alpha_exp);
      if (configVerbose > 0) {
        fprintf(stderr, "%s NSA Markov Estimate: alpha is %.17g.\n", label, alphaT);
      }

      epsilon_term = log(1.0 / (1.0 - alphaT)) / 2.0;
      if (configVerbose > 3) {
        fprintf(stderr, "%s NSA Markov Estimate: epsilon_term is %.17g.\n", label, epsilon_term);
      }

      // Calculate the epsilon for the symbol probabilities
      epsilon = sqrt(epsilon_term / ((double)validSymbolCount));
      if (configVerbose > 0) {
        fprintf(stderr, "%s NSA Markov Estimate: Epsilon is %.17g.\n", label, epsilon);
      }
    } else {
      epsilon = 0.0;
      epsilon_term = 0.0;
    }

    assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

    /* # 2. Estimate the initial state probability distribution, P, with:
     * # Pi = min(1, o_i/validSymbolCount + epsilon)
     */
    for (i = 0; i < k; i++) {
      if ((count[i] > 0) && (count[i] >= countCutoff)) {
        curprob = ((double)count[i]) / ((double)validSymbolCount) + epsilon;
        assert(curprob >= 0.0);

        if (curprob > 1.0) {
          curprob = 1.0;
        }

        assert(curprob > 0.0);
        P[i] = -log2(curprob);

        if (configVerbose > 3) {
          if (isfinite(P[i])) fprintf(stderr, "%s NSA Markov Estimate: P[%zu] = %.17g\n", label, i, pow(2.0, -P[i]));
        }
      } else {
        P[i] = DBL_INFINITY;
      }
    }

    assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

    /*# 3. Remove one count from the last symbol.
     *     o_{S_L} -- (where, in the specification, S is not zero-indexed...)
     */
    if (count[S[L - 1]] > 0) {
      reducedTrailingSymbolCount = true;
      count[S[L - 1]]--;
    } else {
      reducedTrailingSymbolCount = false;
    }

    /*# 4. Estimate the probabilities in the transition matrix S, overestimating where...
     *    T_{i,j} = 1 if o_i = 0 (This only occurs if this is the last symbol)
     *           min(1, o_{i,j}/o_i + eps_i) otherwise
     */

    if ((configVerbose > 0) && (countCutoff > 0)) fprintf(stderr, "Determining which symbols to populate within the transition matrix.\n");

    // Populate the transition matrix, a row at a time
    for (i = 0; i < k; i++) {
      if ((count[i] > 0) && (count[i] >= countCutoff)) {
        double epsilon_i = 0.0;  // the epsilon_i value
        size_t curRowPop = 0;

        // Count how many transitions from this state to valid symbols there are.
        // This is the total number of counted instances of this current symbol.
        for (j = 0; j < k; j++) {
          if (count[j] >= countCutoff) {
            // The symbol we are transitioning to
            curRowPop += oij[i * k + j];
          }
        }

        if (count[i] != curRowPop) {
          // We apparently changed the acceptable target states, so the current count needs to be updated, and we'll need to redo the matrix construction.
          count[i] = curRowPop;
          isStable = false;
          if (hasNotIterateInformed && (configVerbose > 0)) {
            fprintf(stderr, "A symbol transitioned to an uncommon symbol. Iterating...\n");
            hasNotIterateInformed = false;
          }
        }

        if ((curRowPop > 0) && (curRowPop >= countCutoff)) {
          if (conservative) {
            /*We first populate the epsilon_i value.
             * Note that if o_i is 0, the value of epsilon_i doesn't matter, so our choice of 0.0 was arbitrary.
             */
            epsilon_i = sqrt(epsilon_term / ((double)curRowPop));
            if (maxEpsilon < epsilon_i) maxEpsilon = epsilon_i;
          }

          assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

          // Now that we have a value for epsilon_i, we can populate the transition matrix
          // Calculate the jth entry in the ith row...
          for (j = 0; j < k; j++) {
            if ((count[j] > 0) && (count[j] >= countCutoff)) {
              // We know that curRowProp > 0, so the probability that the ith symbol occurs is non-zero.
              curprob = (((double)oij[i * k + j]) / ((double)curRowPop)) + epsilon_i;

              if (curprob > 1.0) {
                curprob = 1.0;
              }

              if (curprob > 0.0) {
                T[i * k + j] = -log2(curprob);
              } else {
                T[i * k + j] = DBL_INFINITY;
              }
            } else {
              T[i * k + j] = DBL_INFINITY;
            }
            assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
          }
        } else {
          for (j = 0; j < k; j++) T[i * k + j] = DBL_INFINITY;
        }
      } else {
        for (j = 0; j < k; j++) T[i * k + j] = DBL_INFINITY;
      }
    }  // for, iterating over rows

    // If the trailing symbol was reduced, put it back in for the loop.
    if (reducedTrailingSymbolCount) count[S[L - 1]]++;

    if (isStable && conservative && (configVerbose > 2)) {
      fprintf(stderr, "%s NSA Markov Estimate: Maximum Epsilon_i is %.17g.\n", label, maxEpsilon);
      if (maxEpsilon > DESIRABLE_MAX_EPSILON) {
        fprintf(stderr, "%s NSA Markov Estimate: Consider setting cutoff to at least %.17g.\n", label, epsilon_term / (DESIRABLE_MAX_EPSILON * DESIRABLE_MAX_EPSILON * (double)validSymbolCount));
      }
    }
  }  // while; iterate until stable

  // Now the transition matrix includes only transitions from suitably numerous symbols to
  // suitably numerous symbols.

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

  if (configVerbose > 3) {
    for (i = 0; i < k; i++)
      for (j = 0; j < k; j++) {
        if (isfinite(T[i * k + j])) fprintf(stderr, "%s NSA Markov Estimate: T[%zu][%zu] = %.17g\n", label, i, j, pow(2.0, -T[i * k + j]));
      }
  }

  assert(count != NULL);
  free(count);
  count = NULL;

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

  /* # 5. Using the transition matrix T, find the probability of the most likely
     #    sequence of states ...
     # Run time O(k n^2)
   */

  for (j = 0; j < d - 1; j++) {
    /*Each step overwrites h; this keeps track of the prob if we choose c as a next step.*/
    for (c = 0; c < k; c++) { /*for each possible choice of next step*/
      size_t indexchosen;
      /*If we were in state i with prob P[i], calculate Pp[i], the prob to transition to state c*/
      for (i = 0; i < k; i++) {
        Pp[i] = P[i] + T[i * k + c];
      }
      /*Remember the highest prob associated with transitioning to state c
        This is effectively a path choice to c*/
      indexchosen = SIZE_MAX;
      h[c] = mindouble(Pp, k, &indexchosen);
      assert(indexchosen != SIZE_MAX);
    }

    /*h now contains a list of list of the highest prob possible for each state h[c] for c in 0 to k-1*/
    /*This becomes our new P*/
    tempdoubleptr = P;
    P = h;
    h = tempdoubleptr;
  }

  chain_minentropy = mindouble(P, k, NULL);
  assert(chain_minentropy >= 0);

  chain_minentropy = fabs(chain_minentropy);  //-0 arises

  free(oij);
  free(T);
  free(P);
  free(Pp);
  free(h);

  /*This min effectively chooses the final state*/
  result = chain_minentropy / ((double)d);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions == FE_UNDERFLOW) {
    fprintf(stderr, "%s NSA Markov Estimate: Found an Underflow in historic Markov estimator: (result: %.17g ignored and set to 0.0)\n", label, result);
    feclearexcept(FE_UNDERFLOW);
    assert(result < RELEPSILON);
    result = 0.0;
  } else if (exceptions != 0) {
    fprintf(stderr, "%s NSA Markov Estimate: Math error in NSA Markov estimator: ", label);
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  if (configVerbose > 0) {
    fprintf(stderr, "%s NSA Markov Estimate: p = %.17g\n", label, pow(2.0, -chain_minentropy));
  }

  return result;
}
