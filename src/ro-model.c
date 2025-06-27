/* This file is part of the Theseus distribution.
 * Copyright 2021-2024 Joshua E. Hill <josh@keypair.us>
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
#include <inttypes.h>
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
#define KSMAXFLIPQUALITY 0.1054040645372386121207L

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "ro-model [-v] [-J] [-B] [-S] [-K] [-W] [-g gaussian-prop] <sigma>\n");
  fprintf(stderr, "Produce a min entropy estimate using the selected stochastic model.\n");
  fprintf(stderr, "Sigma is the observed normalized (period length) jitter standard deviation (expressed as a proportion of the ring oscillator period).\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-J\tUse the Jackson stochastic model.\n");
  fprintf(stderr, "-B\tUse the BLMT stochastic model.\n");
  fprintf(stderr, "-S\tUse the Saarinen stochastic model.\n");
  fprintf(stderr, "-K\tUse the Killmann-Schindler stochastic model.\n");
  fprintf(stderr, "-W\tUse the worst-case Killmann-Schindler stochastic model.\n");
  fprintf(stderr, "-g <r>\tOperate under the assumption that the unpredictable portion of the observed jitter is sigma*r (so reduce the observed jitter by the factor r).\n");
  exit(EX_USAGE);
}

/*This is the model that Ben Jackson presented at the CMUF Entropy WG meeting on 20190618*/
/*Strengths of this model:
 * Its outputs are well aligned with the results of 90B testing.
 * Its outputs are consistent with a fairly powerful attacker.
 *  * It presumes that the attacker knows the initial state prior to each sample cycle (but cannot control it)
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance).
 *
 *Weaknesses:
 * It presumes that the output of the RO is serially XORed in pairs.
 *  * If this isn't true, then the produced estimate is an approximation.
 * It presents an average case (results are averaged over all initial phases).
 *   Assumption is that the per-sample starting phase is uniformly distributed.
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
 * Note that:
 *   The modelGFunction(x,y)=x*erf(x) - y*erf(y) + (exp(-x*x)-exp(-y*y))/sqrt(Pi) is even in both x and y.
 *   That is modelGFunction(x,y) = modelGFunction(-x,y) = modelGFunction(x,-y) = modelGFunction(-x,-y)
 *     b_i = i/sqrt(2*sigma^2) is an odd function in i, as b_{-i} = - b_{i}.
 *     a_i = (i+1/2)/sqrt(2*sigma^2) is "shifted odd", as a_{-(i+1)} = - a_{i}.
 *     c_i = (i-1/2)/sqrt(2*sigma^2) is related to a_i, as a_{-i} = - c_{i} and c_{-i} = - a_{i}
 * These allow for various groupings of the truncated sum, which runs \sum_{i=-cutoff}^{cutoff}, with the end result being that we
 * only need to sum over the positive terms, and then correct.
 */

/*This function represents a common term that arises when integrating the normal distribution CDF.
 *This function is used in several models (the Jackson, Saarinen, and Killmann-Schindler models)*/
/*We get a lot of symmetry as a result of this function; it is even in both x and y!
 *That is modelGFunction(x,y) = modelGFunction(-x,y) = modelGFunction(x,-y) = modelGFunction(-x,-y)
 */
static long double modelGFunction(long double x, long double y) {
  long double result;

  result = x * erfl(x) - y * erfl(y) + (expl(-x * x) - expl(-y * y)) * SQRTPIINVL;

  if (configVerbose > 3) fprintf(stderr, "G(x=%.22Lg, y=%.22Lg) = %.22Lg\n", x, y, result);
  return result;
}

static long double JacksonSummand(long double bn, long double delta) {
  // const long double an = bn + 0.5L*delta;
  // const long double cn = bn - 0.5L*delta;
  long double result;

  result = modelGFunction(bn + 0.5L * delta, bn) + modelGFunction(bn - 0.5L * delta, bn);  // G(a_n, b_n) + G(c_n, b_n)

  if (configVerbose > 2) fprintf(stderr, "Jackson F = %.22Lg\n", result);
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

  cutoff = (int)fmaxl(ceil(10.0L * sigma), 1.0L);
  if (configVerbose > 1) fprintf(stderr, "Truncating Jackson model sum to %d terms\n", 2 * cutoff + 1);

  // This calculates \sum_{j=-cutoff}^cutoff F_j
  // We have a great deal of symmetry here that we can use to sum the F_j and F_{-j} terms at the same time.
  for (int j = cutoff; j > 0; j--) {
    long double summand;

    // calculate b_{j} and calculate the current summand.
    // Note, we could just repeatedly subtract, but we accumulate floating point errors doing this.
    summand = JacksonSummand(((long double)j) * sigmaTermInv, sigmaTermInv);

    if (isnormal(summand)) {
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

  if (configVerbose > 1) fprintf(stderr, "Jackson tuplePmax = %.22Lg\n", tuplePmax);

  perBitPmax = (1.0L + sqrtl(2.0L * tuplePmax - 1.0L)) / 2.0L;
  if (configVerbose > 1) fprintf(stderr, "Jackson perBitPmax = %.22Lg\n", perBitPmax);

  averageEntropy = -log2l(perBitPmax);
  if (configVerbose > 2) fprintf(stderr, "Jackson Averaged Per-Bit Min Entropy = %.22Lg\n", averageEntropy);

  return averageEntropy;
}

// This is from the paper "On Entropy and Bit Patterns of Ring Oscillator Jitter" by Markku-Juhani O. Saarinen
// https://arxiv.org/abs/2102.02196
/*Strengths of this model:
 * Its outputs are well aligned with the results of 90B testing.
 * Its outputs are consistent with a fairly powerful attacker.
 *  * It presumes that the attacker knows the initial state prior to each sample cycle (but cannot control it).
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance).
 *
 *Weaknesses:
 * It presents an average case (results are averaged over all initial phases).
 *   Assumption is that the per-sample starting phase is uniformly distributed.
 */
static long double SaarinenSummand(long double bn, long double delta) {
  // const long double an = bn + 0.5L*delta;
  long double result;

  result = modelGFunction(bn + 0.5L * delta, bn);
  if (configVerbose > 2) fprintf(stderr, "SaarinenS(b_n=%.22Lg, 1/sqrt(2*sigma^2)=%.22Lg) = %.22Lg\n", bn, delta, result);

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

  cutoff = (int)fmaxl(ceil(10.0L * sigma), 1.0L);
  if (configVerbose > 1) fprintf(stderr, "Truncating Saarinen model sum to %d terms\n", 2 * cutoff + 1);

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

    if (isnormal(summand)) {
      // This captures (in rough terms) both the jth term, along with parts of the the -(j-1)th term.
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
  if (configVerbose > 1) fprintf(stderr, "Saarinen S_1(1/2; F=0, D=1/2) = %.22Lg\n", sum);

  pe = 4.0L * sum;

  if (configVerbose > 1) fprintf(stderr, "Saarinen p_e = %.22Lg\n", pe);

  entropy = -log2l(pe);
  if (configVerbose > 2) fprintf(stderr, "Saarinen Per-Bit Min Entropy Lower Bound = %.22Lg\n", entropy);

  return entropy;
}

/*This is from the paper "On the Security of Oscillator-Based Random Number Generators, by Baudet, Lubicz, Micolod, and Tassiaux.
 *This model produces Shannon Entropy. We interpret this value as a per-bit value (which for this bound, is reasonable, but would
 *not work out for other bounds in that paper.)
 *Given the Shannon entropy for a specific bit, we can determine what p_max must have been in order to get this Shannon Entropy,
 *and then we use this to calculate a min entropy.
 */
/*Strengths of this model:
 * It is the basis of much of the work in this area.
 * It seems to be generally accepted in AIS-31.
 *  See Petura, Mureddu, Bochard, Fischer, Bossuet "A Survey of AIS-20/31 Compliant TRNG Cores Suitable for FPGA Devices".
 *
 *Weaknesses:
 * This model uses a truncated series to create these estimates, but truncating at the first two terms provides only the dominant
 * effects jitter. (I suspect that this is a major reason for bias accumulation in a high-jitter oscillator).
 * This model cannot produce a value lower than 0.415298 for Shannon Entropy, which translates to approximately 0.12 bits of min entropy.
 *      * This really highlights how this model isn't a good fit for most deployed systems. A wholly deterministic ring oscillator that is
 *        regularly sampled ought to be credited with 0 entropy.
 * Examining the results from this model, it seems that this model breaks down when sigma<0.1 (Q<0.01).
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

  if (sigma <= 0.1L) fprintf(stderr, "The BLMT model returns inaccurate results when sigma < 0.1. It would be better to use some other model for this parameter.\n");

  shannonEntropy = 1.0L - 4.0L / (pisq * logl(2.0L)) * expl(-4.0L * pisq * sigmasq);
  if (shannonEntropy < 0.0L) {
    fprintf(stderr, "BLMT Assessment of Shannon Entropy is lower than possible.\n");
    return 0.0L;
  }

  if (shannonEntropy > 1.0L) {
    fprintf(stderr, "BLMT Assessment of Shannon Entropy is higher than possible.\n");
    return 1.0L;
  }

  if (configVerbose > 1) fprintf(stderr, "BLMT Model predicts a Shannon Entropy of %.22Lg\n", shannonEntropy);

  foundPmax = monotonicBinarySearch(ShannonEst, 0.5, 1.0, (double)shannonEntropy, NULL, true);
  if (configVerbose > 1) fprintf(stderr, "BLMT inferred p_max = %.17g\n", foundPmax);

  minEntropy = -log2(foundPmax);

  return (long double)minEntropy;
}

/* A support function for the models arising from the Killmann-Schindler paper, and its direct descendants.
 * This requires a function that (for the given parameter) can calculate the probability of a
 * certain number of flips (or "flops" if you are reading the Ma-Lin-Chen-Xu-Liu-Jing paper) having occurred in a
 * certain amount of time. It then groups the values by the least significant bit of the flip count
 * (which establishes the output symbol), and uses this to calculate the min entropy of the source.
 */
static long double cycleCountLsbNormalizedMinEnt(long double flipSigma, long double (*cycleProbFunction)(uint64_t, long double)) {
  const uint64_t expectedCycles = 2000;
  uint64_t delta = (uint64_t)ceil(fmax(sqrtl((long double)expectedCycles) * flipSigma * 10, 1.0L));
  long double evenP = 0.0L;
  long double oddP = 0.0L;
  long double extraP = 0.0L;
  long double centerProb;
  long double lastMaxValue = 0.0L;

  if(configVerbose > 4) fprintf(stderr, "KS flip-sigma: %.22Lg\n", flipSigma);

  if(flipSigma > KSMAXFLIPQUALITY) return 1.0L;

  // The cycle count distribution is asymptotically normal, but the standard deviation isn't that easy to compute
  //(See equation 28 for details)
  // This re-computes some of the same terms more than once (which makes me feel bad about myself), but it
  // doesn't take that long to re-compute them.
  // We use DBL_EPSILON here because, in testing, the accumulated floating point error was well above LDBL_EPSILON,
  // And waiting until we found numbers that small ended up producing some rather strange behavior that triggered
  // some asserts. By being somewhat less ambitious on our accuracy goal, we avoid a bunch of bad behavior.
  // We also ultimately only print out results using a double's accuracy.
  while (cycleProbFunction(expectedCycles + delta, flipSigma) > (long double)DBL_EPSILON) delta++;
  while ((delta <= expectedCycles) && (cycleProbFunction(expectedCycles - delta, flipSigma) > (long double)DBL_EPSILON)) delta++;
  if (configVerbose > 2) fprintf(stderr, "Final delta: %" PRIu64 "\n", delta);

  // We don't expect strict symmetry, but the distribution is vaguely symmetric, distributed around its center bin.
  // Sum the values from smallest to largest in absolute magnitude.
  // This should help reduce floating point accumulation error.
  for (uint64_t j = delta; j > 0; j--) {
    long double roundPHigh = cycleProbFunction(expectedCycles + j, flipSigma);
    long double roundPLow;
    long double roundCombined;
    long double curMaxValue;

    if(j <= expectedCycles) {
      roundPLow = cycleProbFunction(expectedCycles - j, flipSigma);
    } else {
      roundPLow = 0.0L;
    }

    // Note that (expectedCycles + j) and (expectedCycles - j) have the same lsb
    // That is, they are both even or both odd.
    roundCombined = roundPHigh + roundPLow;

    curMaxValue = fmax(roundPHigh, roundPLow);
    assert((fabsl(curMaxValue-lastMaxValue)/fmax(curMaxValue,lastMaxValue) <= (long double)DBL_EPSILON) || (lastMaxValue <= curMaxValue));  // Verify that we're generally increasing
    lastMaxValue = curMaxValue;

    if (((expectedCycles + j) & 1) == 0) {
      evenP += roundCombined;
    } else {
      oddP += roundCombined;
    }
  }

  // Now add in the probability for the center bin
  centerProb = cycleProbFunction(expectedCycles, flipSigma);

  assert((fabsl(centerProb-lastMaxValue)/fmax(centerProb,lastMaxValue) <= (long double)DBL_EPSILON) || (lastMaxValue <= centerProb));  // Verify that we're generally increasing

  if ((expectedCycles & 1) == 0) {
    evenP += centerProb;
  } else {
    oddP += centerProb;
  }

  extraP = 1.0L - evenP - oddP;
  if (configVerbose > 1) fprintf(stderr, "evenP: %.22Lg, oddP: %.22Lg, extraP: %.22Lg\n", evenP, oddP, extraP);

  return -log2l(fmin(fmax(evenP, oddP) + extraP, 1.0L));
}

/* This follows Killmann-Schindler "A Design for a Physical RNG with Robust Entropy Estimators".
 * The model presented is very general, and can easily be converted into a min entropy estimate naturally.
 */
/*Strengths of this model:
 * It assumes a fairly powerful attacker.
 *  * It presumes that the attacker knows the initial state prior to each sample cycle (but cannot control it).
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance).
 *
 *Weaknesses:
 * It presents an average case (results are averaged over all initial phases).
 *   Assumption is that the per-sample starting phase is uniformly distributed.
 *
 * Note: All the formulas in this implementation use notation and conventions as specified in the Killmann-Schindler
 * paper, but this paper is counting full cycles (0->1 crossings). This isn't exactly what we want, as the output
 * of the periodically sampled ring oscillator isn't the lsb of the cycle count, it is the lsb of the
 * flip count (half cycles). As such, when these formulas are actually used, everything needs to be converted to
 * the corresponding flipping statistics.
 */

/*
 * This is a function that is provided to cycleCountLsbNormalizedMinEnt.
 *
 * This function is essentially equation (22) from that paper, but after applying a estimate noted in
 * Ma-Lin-Chen-Xu-Lie-Jing "Entropy Evaluation for Oscillator-based True Random Number Generators",
 * equation (7). Note that this function's derivative has a non-zero value only when its argument is
 * less than mu, so the Riemann-Stieltjes integral's bounds can be adjusted to 0 to mu.
 * We also note that for large k, sqrt(k) is approximately equal to sqrt(k+1).
 * We're normalizing everything so that mu=1/2 (remember, this is the average flipping time) and s=1000.
 * The result of symbolically integrating this expression can be written in closed form (using special functions)
 * in terms of our G function.
 */
static long double KillmannSchindlerNormalizedCycleProb(uint64_t kin, long double sigma) {
  long double result;
  long double k = (long double)kin;

  assert(kin > 1);

  result = 2.0L * sqrtl((k - 1.0L)) * sigma / sqrtl(2.0L) *
           (modelGFunction((k - 2001.0L) / (2.0L * sqrtl(2.0L * (k - 1.0L)) * sigma), (k - 2000.0L) / (2.0L * sqrtl(2.0L * (k - 1.0L)) * sigma)) +
            modelGFunction((k - 1999.0L) / (2.0L * sqrtl(2.0L * (k - 1.0L)) * sigma), (k - 2000.0L) / (2.0L * sqrtl(2.0L * (k - 1.0L)) * sigma)));

  if (!isnormal(result) || (result < 0.0L)) result = 0.0L;
  if (result > 1.0L) result = 1.0L;

  if (configVerbose > 2) fprintf(stderr, "KSCycleProb(k=%" PRIu64 ", sigma=%.22Lg) = %.22Lg\n", kin, sigma, result);
  return result;
}

static long double KillmannSchindlerModel(long double sigma) {
  return cycleCountLsbNormalizedMinEnt(sigma / sqrtl(2000.0L), KillmannSchindlerNormalizedCycleProb);
}

/* This model is adapted from Killmann-Schindler "A Design for a Physical RNG with Robust Entropy Estimators"
 * This is intended to model the case where an attacker is able to set the initial phase for their maximum
 * benefit. It is, in some sense, a worst-case entropy for a functioning ideal ring oscillator.
 */
/*Strengths of this model:
 * It is very conservative.
 * it assumes a profoundly powerful attacker
 *    * It assumes that the attacker can _set_ the initial state prior to each sample cycle,
 *      and that they set it to the worst-case initial phase (that is a phase that makes the resulting
 *      output the most predictable).
 * Its large-scale characteristics are consistent with our expectations (it produces 0 for deterministic ROs, for instance).
 *
 *Weaknesses:
 * The results are likely artificially low with respect to most attackers.
 *
 * Note: All the formulas in this implementation use notation and conventions as specified in the Killmann-Schindler
 * paper, but this paper is counting full cycles (0->1 crossings). This isn't exactly what we want, as the output
 * of the periodically sampled ring oscillator isn't the lsb of the cycle count, it is the lsb of the
 * flip count (half cycles). As such, when these formulas are actually used, everything needs to be converted to
 * the corresponding flipping statistics.
 */

/*
 * This function is equation (17) from Killmann-Schindler, where x=v*mu.
 * Note that for large k, sqrt(k) is approximately equal to sqrt(k+1).
 */
static long double KillmannSchindlerResetCycleProb(long double x, uint64_t kin, long double mu, long double sigma) {
  long double k = (long double)kin;
  long double result;

  result = 0.5L * erfcl(-((x - k * mu) / (sqrtl(2.0L) * sqrtl(k) * sigma))) - 0.5L * erfcl(-((x - (1.0L + k) * mu) / (sqrtl(2.0L) * sqrtl(k) * sigma)));

  if (!isnormal(result) || (result < 0.0L)) result = 0.0L;
  if (result > 1.0L) result = 1.0L;

  if (configVerbose > 2) fprintf(stderr, "KSResetCycleProb(x=%.22Lg, k=%" PRIu64 ", mu=%.22Lg, sigma=%.22Lg) = %.22Lg\n", x, kin, mu, sigma, result);
  return result;
}

/*
 * This is a function that is provided to cycleCountLsbNormalizedMinEnt.
 *
 * Constructs the "worst-case" (lowest entropy) situation, given the parameters input yield
 * a situation where it is maximally likely that the deterministic output is produced.
 * We're normalizing everything so that mu=1/2 (remember, this is the average flipping time) and s=1000.
 */
static long double KillmannSchindlerLowerBoundNormalizedCycleProb(uint64_t k, long double sigma) {
  const long double s = 1000.0L;
  const long double mu = 0.5L;  // Each cycle has mean unit length, but the flipping time is 1/2
  const long double nu = 0.5L;  // Target flipping phase 1/2 (which corresponds to cycle phase of 1/4 or 3/4)

  assert(k > 1);

  // Adjust the sample period so that the deterministic outcome is the worst-case (i.e., the furthest from any transition).
  // This occurs at the normalized phase (where the flips occur at integers) of 0.5 (0.25 for the full period)
  return KillmannSchindlerResetCycleProb(floor(s / mu) * mu + nu * mu, k, mu, sigma);
}

static long double KillmannSchindlerLowerBoundModel(long double sigma) {
  return cycleCountLsbNormalizedMinEnt(sigma / sqrtl(2000.0L), KillmannSchindlerLowerBoundNormalizedCycleProb);
}

int main(int argc, char *argv[]) {
  int opt;
  bool configJackson = false;
  bool configBLMT = false;
  bool configSaarinen = false;
  bool configKillmannSchindler = false;
  bool configKillmannSchindlerLowerBound = false;
  long double sigma;
  long double gaussianProp = 1.0L;
  char *nextArgument;

  configVerbose = 0;

  if (argc < 2) {
    useageExit();
  }

  sigma = strtold(argv[argc - 1], &nextArgument);
  if ((errno == ERANGE) || !isfinite(sigma) || (sigma < 0.0L) || (nextArgument == argv[argc - 1])) {
    fprintf(stderr, "Provided argument is invalid or out of range.\n");
    useageExit();
  }

  argc--;

  while ((opt = getopt(argc, argv, "vJBSKWg:")) != -1) {
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
      case 'K':
        configKillmannSchindler = true;
        break;
      case 'W':
        configKillmannSchindlerLowerBound = true;
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

  if (!(configJackson || configBLMT || configSaarinen || configKillmannSchindler || configKillmannSchindlerLowerBound)) {
    useageExit();
  }

  if (configVerbose > 0) fprintf(stderr, "Provided observed jitter standard deviation %.17Lg\n", sigma);

  if (gaussianProp < 1.0L) {
    sigma = sigma * gaussianProp;
    if (configVerbose > 0) fprintf(stderr, "Presumed proportion of observed jitter due to local Gaussian noise: %.17Lg\n", gaussianProp);
    if (configVerbose > 0) fprintf(stderr, "Looking for entropy for a normalized jitter standard deviation of %.17Lg\n", sigma);
  }

  if (configJackson) printf("Min entropy assessment (Ben Jackson's XORed Average): %.17Lg\n", JacksonModel(sigma));
  if (configBLMT) {
    if (sigma < 0.10L) {
      printf("Overestimate for ");
    }
    printf("Min entropy assessment (BLMT): %.17Lg\n", BLMTModel(sigma));
  }
  if (configSaarinen) printf("Min entropy assessment (Markku-Juhani Saarinen's Lower Bound): %.17Lg\n", SaarinenModel(sigma));
  if (configKillmannSchindler) printf("Min entropy assessment (Killmann-Schindler Average): %.17Lg\n", KillmannSchindlerModel(sigma));
  if (configKillmannSchindlerLowerBound) printf("Min entropy assessment (Killmann-Schindler Lower Bound): %.17Lg\n", KillmannSchindlerLowerBoundModel(sigma));

  return (EX_OK);
}
