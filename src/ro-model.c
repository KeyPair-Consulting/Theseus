/* This file is part of the Theseus distribution.
 * Copyright 2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <fenv.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <tgmath.h>
#include <unistd.h>

#include "fancymath.h"
#include "globals-inst.h"
#include "precision.h"

#define PIL 3.141592653589793238463L
#define SQRTPIINVL 0.5641895835477562869481L  // 1/sqrt(Pi)

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "ro-model [-v] [-J] [-B] [-S] [-g gaussian-prop] <sigma>\n");
  fprintf(stderr, "Produce a min entropy estimate using the selected stochastic model.\n");
  fprintf(stderr, "Sigma is the observed normalized (period length) jitter standard deviation (expressed as a proportion of the ring oscillator period).\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-J\tUse Jackson stochastic model.\n");
  fprintf(stderr, "-B\tUse BLMT stochastic model.\n");
  fprintf(stderr, "-S\tUse Saarinen stochastic model.\n");
  fprintf(stderr, "-g <r>\tOperate under the assumption that the unpredictable portion of the observed jitter is sigma*r (so reduce the observed jitter by the factor r).\n");
  exit(EX_USAGE);
}

/*This is the model that Ben Jackson presented at the CMUF Entropy WG meeting on 20190618*/
/*Strengths of this model:
 * its outputs are well aligned with the results of 90B testing.
 * its outputs are consistent with a fairly powerful attacker
 * 	* It presumes that the attacker knows the initial state prior to each sample cycle.
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance)
 *
 *Weaknesses:
 * It presumes that the output of the RO is serially XORed in pairs.
 * 	* If this isn't true, then the produced estimate is an approximation.
 * It presents an average case (results are averaged over all initial phases).
 */
/*Implementation notes:
 * After the Saarinen model was published, I noticed that a bunch of the terms from that model occur in the
 * Jackson model as well, and many of the computational refinements that I made when implementing the Saarinen
 * model apply to the Jackson model as well.
 *
 * This model sums from -cutoff to cutoff, but we make use of some symmetry and sum from the outside summands (which
 * are expected to be small) to the middle summands (whose terms are expected to be large.)
 * This is done in an attempt at reducing floating point error accumulation.
 *
 * Note that this implementation of the Jackson and the Saarinen models rely on some symmetry in the positive and
 * negative summands that make up their sums.
 * Note that
 * 	The modelGFunction(x,y)=x*erf(x) - y*erf(y) + (exp(-x*x)-exp(-y*y))/sqrt(Pi) is even in both x and y.
                That is modelGFunction(x,y) = modelGFunction(-x,y) = modelGFunction(x,-y) = modelGFunction(-x,-y)
 * 	b_i = i/sqrt(2*sigma^2) is an odd function in i, as b_{-i} = - b_{i}.
 * 	a_i = (i+1/2)/sqrt(2*sigma^2) is "shifted odd", as a_{-(i+1)} = - a_{i}.
 * 	c_i = (i-1/2)/sqrt(2*sigma^2) is related to a_i, as a_{-i} = - c_{i} and c_{-i} = - a_{i}
 * These allow for various groupings of the truncated sum, which runs \sum_{i=-cutoff}^{cutoff}, with the end result being that we
 * only need to sum over the positive terms, and then correct.
 */

/*This function outputs a function that is in both the Jackson and Saarinens models*/
/*We get a lot of symmetry as a result of this function; it is even in both x and y!*/
static long double modelGFunction(long double x, long double y) {
  long double term1, term2, term3, result;

  term1 = x * erfl(x);
  term2 = -y * erfl(y);
  term3 = (expl(-x * x) - expl(-y * y)) * SQRTPIINVL;
  if (configVerbose > 2) fprintf(stderr, "G summand term1 = %.22Lg\n", term1);
  if (configVerbose > 2) fprintf(stderr, "G summand term2 = %.22Lg\n", term2);
  if (configVerbose > 2) fprintf(stderr, "G summand term3 = %.22Lg\n", term3);
  result = term1 + term2 + term3;

  if (configVerbose > 1) fprintf(stderr, "G(x=%.22Lg, y=%.22Lg) = %.22Lg\n", x, y, result);
  return result;
}

static long double JacksonSummand(long double bn, long double delta) {
  // const long double an = bn + 0.5L*delta;
  // const long double cn = bn - 0.5L*delta;
  long double result;

  result = modelGFunction(bn + 0.5L * delta, bn) + modelGFunction(bn - 0.5L * delta, bn);  // G(a_n, b_n) + G(c_n, b_n)

  if (configVerbose > 1) fprintf(stderr, "Jackson F = %.22Lg\n", result);
  return result;
}

static long double JacksonModel(long double sigma) {
  int cutoff;
  long double tuplePmax = 0.0L;
  long double perBitPmax;
  long double averageEntropy;
  const long double sigmaTerm = sigma * sqrtl(2.0L);
  const long double sigmaTermInv = 1.0L / sigmaTerm;

  if (sigma <= 0.0L) return 0.0L;

  cutoff = (int)ceil(7.0L * sigma);

  // This calculates \sum_{j=-cutoff}^cutoff F_j
  // We have a great deal of symmetry here that we can use to sum the F_j and F_{-j} terms at the same time.
  for (int j = cutoff; j > 0; j--) {
    long double summand;

    // calculate b_{j} and calculate the current summand.
    // Note, we could just repeatedly subtract, but we accumulate floating point errors doing this.
    summand = JacksonSummand(((long double)j) * sigmaTermInv, sigmaTermInv);

    if (summand >= LDBL_MIN) {
      tuplePmax = tuplePmax + 2.0L * summand;
    } else {
      feclearexcept(FE_UNDERFLOW);
    }
  }

  // b_0 is 0.
  // This term does not get doubled.
  tuplePmax += JacksonSummand(0.0L, sigmaTermInv);

  // Now apply the scalar for sigma
  tuplePmax *= sigmaTerm;

  if (configVerbose > 0) fprintf(stderr, "Jackson tuplePmax=%.22Lg\n", tuplePmax);

  perBitPmax = (1.0L + sqrtl(2.0L * tuplePmax - 1.0L)) / 2.0L;
  if (configVerbose > 0) fprintf(stderr, "Jackson perBitPmax=%.22Lg\n", perBitPmax);

  averageEntropy = -log2l(perBitPmax);
  if (configVerbose > 1) fprintf(stderr, "Jackson Averaged Per-Bit Min Entropy = %.22Lg\n", averageEntropy);

  return averageEntropy;
}

// This is from the paper "On Entropy and Bit Patterns of Ring Oscillator Jitter" by Markku-Juhani O. Saarinen
// https://arxiv.org/abs/2102.02196
/*Strengths of this model:
 * its outputs are well aligned with the results of 90B testing.
 * its outputs are consistent with a fairly powerful attacker
 * 	* It presumes that the attacker knows the initial state prior to each sample cycle.
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance)
 *
 *Weaknesses:
 * It presents an average case (results are averaged over all initial phases).
 */
static long double SaarinenSummand(long double bn, long double delta) {
  // const long double an = bn + 0.5L*delta;
  long double result;

  result = modelGFunction(bn + 0.5L * delta, bn);
  if (configVerbose > 1) fprintf(stderr, "SaarinenS(b_n=%.22Lg, 1/sqrt(2*sigma^2) = %.22Lg) = %.22Lg\n", bn, delta, result);

  return result;
}

/* These are sums from -cutoff to cutoff, but we make use of some symmetry in the sum and work from the outside terms (which
 * are expected to be small) to the middle terms (whose terms are expected to be large.)
 * This is done in an attempt to reduce floating point error accumulation.
 */
static long double SaarinenModel(long double sigma) {
  int cutoff;
  long double sum = 0.0L;
  long double pe;
  long double entropy;
  long double b_cutoff;
  const long double sigmaTerm = sigma * sqrtl(2.0L);
  const long double sigmaTermInv = 1.0L / sigmaTerm;
  const long double a0 = 0.5L * sigmaTermInv;

  if (sigma <= 0.0L) return 0.0L;

  cutoff = (int)ceil(7.0L * sigma);

  // calculate b_{cutoff}
  b_cutoff = ((long double)cutoff) / (sigma * sqrtl(2.0L));

  /*Here are some terms from the end of the range that aren't picked up in the central sum*/
  sum = SaarinenSummand(b_cutoff, sigmaTermInv) - b_cutoff * erfl(b_cutoff) - expl(-b_cutoff * b_cutoff) * SQRTPIINVL;

  /*Now do the central part of the sum, starting at the outside, and heading to the middle.*/
  for (int j = cutoff - 1; j > 0; j--) {
    long double summand;

    // calculate b_{j} and use it to get the summand.
    // Note, we could just repeatedly subtract, but we accumulate floating point errors doing this.
    summand = SaarinenSummand(((long double)j) * sigmaTermInv, sigmaTermInv);

    if (summand >= LDBL_MIN) {
      // This captures (in rough terms) both the jth term, along with parts of the the -(j-1) th term.
      sum += 2.0L * summand;
    } else {
      feclearexcept(FE_UNDERFLOW);
    }
  }

  /*Some straggling terms from the middle of the sum.*/
  // b_0 is 0.0L.
  sum += SaarinenSummand(0.0L, sigmaTermInv) + a0 * erfl(a0) + expl(-a0 * a0) * SQRTPIINVL;

  /*Now scale the sum.*/
  sum *= sigma * sqrtl(2.0L) / 2.0L;

  /*sum now contains S_1(1/2)*/
  if (configVerbose > 0) {
    fprintf(stderr, "Saarinen S_1(1/2; F=0, D=1/2)=%.22Lg\n", sum);
  }

  pe = 4.0L * sum;

  if (configVerbose > 0) fprintf(stderr, "Saarinen p_e = %.22Lg\n", pe);

  entropy = -log2l(pe);
  if (configVerbose > 1) fprintf(stderr, "Saarinen Per-Bit Min Entropy Lower Bound = %.22Lg\n", entropy);

  return entropy;
}

/*This is from the paper "On the Security of Oscillator-Based Random Number Generators, by Baudet, Lubicz, Micolod, and Tassiaux
 *This model produces Shannon Entropy. We wave our hands, and interpret this value as a per-bit value (this is not really quite
 *true, as this value reflects an average value). Given the Shannon entropy for a specific bit, we can determine what p_max
 *must have been in order to get this Shannon Entropy, and then we use this to calculate a min entropy.
 */
/*Strengths of this model:
 * It is the basis of much of the work in this area.
 * It seems to be generally accepted in AIS-31.
 * 	See Petura, Mureddu, Bochard, Fischer, Bossuet "A Survey of AIS-20/31 Compliant TRNG Cores Suitable for FPGA Devices".
 *
 *Weaknesses:
 * It assumes an initial uniform distribution of phases, and presumes that an attacker can never do better than this when starting.
 * 	* It assumes that the attack does not know the starting phase.
 *  * It assumes that between calls there is sufficient time to allow this uniform distribution to be the best an attacker can do.
 *    * As a practical matter, most sources don't enforce this sort of entropy accumulation "black out" period beyond the sampling frequency.
 * 	* In this model much of the produced entropy is actually due to solely this initial state.
 * 		* (n.b. the long term bias accumulates without bound with the number of outputs! For jittery ring oscillators, this can't be true).
 * This model presumes that between calls there is sufficient time to allow this uniform distribution
 * This model uses a truncated series to create these estimates, but truncating at the first two terms provides only the dominant
 * effects jitter. (I suspect that this is a major reason for bias accumulation in a high-jitter oscillator).
 * This model cannot produce a value lower than 0.415298 for Shannon Entropy, which translates to approximately 0.12 bits of min entropy.
 * 	* This really highlights how this model isn't a good fit for most deployed systems. A wholly deterministic ring oscillator that is 
 *        regularly sampled ought to be credited with 0 entropy.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
/*The params parameter isn't used here, but it is necessary in other contexts. We want to ignore the compiler's complaints here.*/
static double ShannonEst(double p, const size_t *params) {
  assert(p > 0.0);
  assert(p < 1.0);
  return -(p * log2(p) + (1.0 - p) * log2(1 - p));
}
#pragma GCC diagnostic pop

static long double BLMTModel(long double sigma) {
  const long double pisq = PIL * PIL;
  long double sigmasq = sigma * sigma;
  long double shannonEntropy;
  double foundPmax;
  double minEntropy;

  if (sigma <= 0.0L) {
    sigmasq = 0.0L;
  }

  shannonEntropy = 1.0L - 4.0L / (pisq * logl(2.0L)) * expl(-4.0L * pisq * sigmasq);
  if (shannonEntropy < 0.0L) {
    fprintf(stderr, "BLMT Assessment of Shannon Entropy is lower than possible.\n");
    return 0.0L;
  }

  if (shannonEntropy > 1.0L) {
    fprintf(stderr, "BLMT Assessment of Shannon Entropy is higher than possible.\n");
    return 1.0L;
  }

  if (configVerbose > 0) fprintf(stderr, "BLMT Model predicts a Shannon Entropy of %.22Lg\n", shannonEntropy);

  foundPmax = monotonicBinarySearch(ShannonEst, 0.5, 1.0, (double)shannonEntropy, NULL, true);
  if (configVerbose > 0) fprintf(stderr, "BLMT inferred p_max = %.17g\n", foundPmax);

  minEntropy = -log2(foundPmax);
  if (configVerbose > 1) fprintf(stderr, "BLMT inferred average min entropy %.17g\n", minEntropy);

  return (long double)minEntropy;
}

int main(int argc, char *argv[]) {
  int opt;
  bool configJackson = false;
  bool configBLMT = false;
  bool configSaarinen = false;
  long double sigma;
  long double gaussianProp = 1.0L;
  char *nextArgument;

  configVerbose = 0;

  if (argc < 2) {
    useageExit();
  }

  sigma = strtold(argv[argc - 1], &nextArgument);
  if ((errno == ERANGE) || !isfinite(sigma) || (sigma < 0.0L) || (nextArgument == argv[0])) {
    fprintf(stderr, "Provided argument is invalid or out of range.\n");
    exit(EX_DATAERR);
  }

  argc--;

  while ((opt = getopt(argc, argv, "vJBSg:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'J':
        configJackson = true;
        break;
      case 'B':
        configBLMT = true;
        break;
      case 'S':
        configSaarinen = true;
        break;
      case 'g':
        gaussianProp = strtold(optarg, &nextArgument);
        if ((errno == ERANGE) || !isfinite(gaussianProp) || (gaussianProp < 0.0L) || (gaussianProp > 1.0L) || (nextArgument == optarg)) {
          fprintf(stderr, "Provided argument is invalid or out of range.\n");
          exit(EX_DATAERR);
        }
        break;
      default: /* ? */
        useageExit();
    }
  }

  if (argc != optind) {
    useageExit();
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Provided observed jitter standard deviation %.22Lg\n", sigma);
  }

  if (gaussianProp < 1.0L) {
    sigma = sigma * gaussianProp;
    if (configVerbose > 0) {
      fprintf(stderr, "Presumed proportion of observed jitter due to local Gaussian noise: %.22Lg\n", gaussianProp);
      fprintf(stderr, "Looking for entropy for a normalized jitter standard deviation of %.22Lg\n", sigma);
    }
  }

  if (configJackson) printf("Min entropy assessment (Ben Jackson's XORed Average): %.22Lg\n", JacksonModel(sigma));
  if (configBLMT) printf("Min entropy assessment (BLMT): %.22Lg\n", BLMTModel(sigma));
  if (configSaarinen) printf("Min entropy assessment (Markku-Juhani Saarinen's Lower Bound): %.22Lg\n", SaarinenModel(sigma));

  return (EX_OK);
}
