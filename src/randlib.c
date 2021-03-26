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
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "entlib.h"
#include "fancymath.h"
#include "randlib.h"

#define uint128_t __uint128_t

/* This is xoshiro256** 1.0*/
/*This implementation is derived from David Blackman and Sebastiano Vigna's implementation,
 *which they placed into the public domain.
 *See http://xoshiro.di.unimi.it/xoshiro256starstar.c
 */
static inline uint64_t rotl(const uint64_t x, int k) {
  return (x << k) | (x >> (64 - k));
}

static inline uint64_t xoshiro256starstar(uint64_t *xoshiro256starstarState) {
  const uint64_t result_starstar = rotl(xoshiro256starstarState[1] * 5, 7) * 9;
  const uint64_t t = xoshiro256starstarState[1] << 17;

  xoshiro256starstarState[2] ^= xoshiro256starstarState[0];
  xoshiro256starstarState[3] ^= xoshiro256starstarState[1];
  xoshiro256starstarState[1] ^= xoshiro256starstarState[2];
  xoshiro256starstarState[0] ^= xoshiro256starstarState[3];

  xoshiro256starstarState[2] ^= t;

  xoshiro256starstarState[3] = rotl(xoshiro256starstarState[3], 45);

  return result_starstar;
}

void initGenerator(struct randstate *rstate) {
  assert(rstate != NULL);

  rstate->seeded = false;
  rstate->deterministic = false;
  rstate->buffered32Avail = false;
}

void seedGenerator(struct randstate *rstate) {
  FILE *infp;
  // These default values are HEX versions of PI, E, a nibble count, and log[2].
  uint64_t xoshiro256starstarIni[4] = {0x3243f6a8885a308d, 0x2b7e151628aed2a6, 0x0123456789abcdef, 0xb17217f7d1cf79ab};
  uint32_t MTini[4] = {0x1234, 0x5678, 0x9abc, 0xdef0};

  assert(rstate != NULL);

  if (!rstate->deterministic) {
    if ((infp = fopen("/dev/urandom", "rb")) == NULL) {
      perror("Can't open random source");
      exit(EX_OSERR);
    }

    if (fread(rstate->xoshiro256starstarState, sizeof(uint64_t), 4, infp) != 4) {
      perror("Can't read random seed");
      exit(EX_OSERR);
    }

    if (fread(MTini, sizeof(uint32_t), 4, infp) != 4) {
      perror("Can't read random seed");
      exit(EX_OSERR);
    }

    if (fclose(infp) != 0) {
      perror("Couldn't close random source");
      exit(EX_OSERR);
    }
  } else {
    memcpy(rstate->xoshiro256starstarState, xoshiro256starstarIni, sizeof(uint64_t) * 4);
  }

  sfmt_init_by_array(&(rstate->sfmt), MTini, 4);

  rstate->seeded = true;
}

/*There are 3 basic modes here:
 *1) deterministic mode, where the output is the XOR of deterministically seeded SFMT and xoshiro256** generators
 *2) non-deterministic mode on a platform that supports RdRand, where the output is the XOR of the RdRand and xoshiro256** generators
 *3) non-deterministic mode on a platform with no RdRand support, where the output is the XOR of independently seeded SFMT and xoshiro256** generators
 *
 * xoshiro256** is modern, fast, and fairly well regarded, but it doesn't have some of the nicer equidistribution properties of the SFMT.
 * SFMT is well analyzed, very well regarded (it's a MT variant) but somewhat slow.
 * RDRAND is a high quality DRBG implementation.
 */
uint64_t randomu64(struct randstate *rstate) {
  uint64_t out;
#ifdef RDRND
  int rdrndres;
#endif

  assert(rstate != NULL);
  assert(rstate->seeded);

  if (rstate->deterministic) {
    out = sfmt_genrand_uint64(&(rstate->sfmt));
    out ^= xoshiro256starstar(rstate->xoshiro256starstarState);
  } else {
#ifdef RDRND
    do {
      rdrndres = (int)__builtin_ia32_rdrand64_step((unsigned long long *)&out);
    } while (rdrndres != 1);
#else
    out = sfmt_genrand_uint64(&(rstate->sfmt));
#endif
    out ^= xoshiro256starstar(rstate->xoshiro256starstarState);
  }

  if (configVerbose > 9) fprintf(stderr, "Random Data: 0x%" PRIx64 "\n", out);

  return out;
}

void randomBits(statData_t *data, size_t datalen, struct randstate *rstate) {
  statData_t *curword = data;
  size_t leftover;

  assert(data != NULL);
  assert(rstate != NULL);

  for (size_t i = 0; i < (datalen >> 6); i++) {
    uint64_t currand = randomu64(rstate);
    for (size_t j = 0; j < 64; j++) {
      *curword = (currand >> j) & 0x01UL;
      curword++;
    }
  }

  // Did the user ask for a partial block?
  leftover = datalen & 0x3FUL;
  if (leftover > 0) {
    uint64_t currand = randomu64(rstate);
    for (size_t j = 0; j < leftover; j++) {
      *curword = (currand >> j) & 0x01UL;
      curword++;
    }
  }

  assert(curword >= data);
  assert(((size_t)(curword - data)) == datalen);
}

// This now is based on the randomu64 call, as mixing the u32 and u64 calls were a problem with the SFMT code.
uint32_t randomu32(struct randstate *rstate) {
  uint64_t indata;

  if (rstate->buffered32Avail) {
    rstate->buffered32Avail = false;
    return rstate->buffered32;
  } else {
    indata = randomu64(rstate);
    rstate->buffered32 = (uint32_t)((indata >> 32) & 0xFFFFFFFF);
    rstate->buffered32Avail = true;
    return (uint32_t)(0xFFFFFFFFUL & indata);
  }
}

/*Return an integer in the range [0, high], without modular bias*/
/*This is a slight modification of Lemire's approach, as we want [0,s] rather than [0,s)*/
/*See "Fast Random Integer Generation in an Interval" by Lemire (2018) (https://arxiv.org/abs/1805.10941) */
/* The relevant text explaining the central factor underlying this opaque approach is:
 * "Given an integer x ∈ [0, 2^L), we have that (x × s) ÷ 2^L ∈ [0, s). By multiplying by s, we take
 * integer values in the range [0, 2^L) and map them to multiples of s in [0, s × 2^L). By dividing by 2^L,
 * we map all multiples of s in [0, 2^L) to 0, all multiples of s in [2^L, 2 × 2^L) to one, and so forth. The
 * (i + 1)th interval is [i × 2^L, (i + 1) × 2^L). By Lemma 2.1, there are exactly floor(2^L/s) multiples of s in
 * intervals [i × 2^L + (2^L mod s), (i + 1) × 2^L) since s divides the size of the interval (2^L − (2^L mod s)).
 * Thus if we reject the multiples of s that appear in [i × 2^L, i × 2^L + (2^L mod s)), we get that all
 * intervals have exactly floor(2^L/s) multiples of s."
 *
 * This approach allows us to avoid _any_ modular reductions with high probability, and at worst case one
 * reduction. It's an opaque approach, but lovely.
 */
uint32_t randomRange(uint32_t s, struct randstate *rstate) {
  uint32_t x;
  uint64_t m;
  uint32_t l;

  assert(rstate != NULL);

  x = randomu32(rstate);

  if (unlikely(UINT32_MAX == s)) {
    return x;
  } else {
    s++;  // We want an integer in the range [0,s], not [0,s)
    m = (uint64_t)x * (uint64_t)s;
    l = (uint32_t)m;  // This is m mod 2^32

    if (l < s) {
      uint32_t t = ((uint32_t)(-s)) % s;  // t = (2^32 - s) mod s (by definition of unsigned arithmetic in C)
      while (l < t) {
        x = randomu32(rstate);
        m = (uint64_t)x * (uint64_t)s;
        l = (uint32_t)m;  // This is m mod 2^32
      }
    }

    return (uint32_t)(m >> 32U);  // return floor(m/2^32)
  }
}

/*Return an integer in the range [0, high], without modular bias*/
uint64_t randomRange64(uint64_t s, struct randstate *rstate) {
  uint64_t x;
  uint128_t m;
  uint64_t l;

  x = randomu64(rstate);

  if (unlikely(UINT64_MAX == s)) {
    return x;
  } else {
    s++;  // We want an integer in the range [0,s], not [0,s)
    m = (uint128_t)x * (uint128_t)s;
    l = (uint64_t)m;  // This is m mod 2^64

    if (l < s) {
      uint64_t t = ((uint64_t)(-s)) % s;  // t = (2^64 - s) mod s (by definition of unsigned arithmetic in C)
      while (l < t) {
        x = randomu64(rstate);
        m = (uint128_t)x * (uint128_t)s;
        l = (uint64_t)m;  // This is m mod 2^64
      }
    }

    return (uint64_t)(m >> 64U);  // return floor(m/2^64)
  }
}

/*This produces a bit that is biased; the bias (c) reflects the probability
of a 0, which is (c+1.0)/2.0*/
/*So, c should be in the range [-1, 1]*/
uint32_t genRandBiasedBit(double bias, struct randstate *rstate) {
  if (randomUnit(rstate) < (bias + 1.0) / 2.0)
    return 0;
  else
    return 1;
}

/*This produces an integer that is biased _toward_ zero.
 *Here Pr(0) = p
 *For j != 0, Pr(j) = (1-Pr(0))/(2^bits - 1)*/

/*This is consistent with the "almost uniform" distribution in Haggerty/Draper*/
uint32_t genRandBiasedInt(uint32_t bits, double p, struct randstate *rstate) {
  uint32_t greatestSymbol;

  assert(bits <= 32);
  assert(bits > 0);

  greatestSymbol = (uint32_t)(((uint64_t)1U << ((uint64_t)bits)) - 1ULL);
  // Note that greatestSymbol <= UINT32_MAX
  if (p < (1.0 - p) / ((double)greatestSymbol)) {  // greatestSymbol is the number of total symbols - 1
    if (configVerbose > 4) fprintf(stderr, "Bias too low; resetting to unbiased.\n");
    return (randomRange(greatestSymbol, rstate));
  }

  if (randomUnit(rstate) < p) {
    return 0;
  }

  return randomRange(greatestSymbol - 1, rstate) + 1;
}

// Integrate the normal PDF (with parameters passed in) between l and l+1.
static double integrateIntegerInterval(double mean, double stddev, double l) {
  double result = normalCDF(l + 1, mean, stddev) - normalCDF(l, mean, stddev);
  return result;
}

/*There are five possible maximum symbols; The probability of each is the integral of the
 * relevant normal distribution PDF over one of the following intervals;
 *  let max = 2^bits - 1, m = floor(mean)
 * (-\infty, 1), (max, \infty), (m-1, m), (m, m+1), (m+1, m+2)*/

double truncatedNormalminEnt(double mean, double stddev, uint32_t bits) {
  double maxValue = -1.0;
  double max = (double)((1U << bits) - 1);
  double cur;
  double curMinEnt;

  if (stddev <= 0.0) return 0;

  // Those things digitizing to 0
  //(-\infty, 1)
  maxValue = normalCDF(1, mean, stddev);
  if (configVerbose > 3) fprintf(stderr, "(-Infinity, 1): %.17g\n", maxValue);

  // Those things digitizing to max
  //(max, \infty)
  cur = 1.0 - normalCDF(max, mean, stddev);
  if (maxValue < cur) maxValue = cur;
  if (configVerbose > 3) fprintf(stderr, "(max, Infinity): %.17g\n", cur);

  // Those things digitizing to m-1, where m=floor(mean)
  //(m-1, m)
  if ((mean >= 1.0) && (mean <= max)) {
    cur = integrateIntegerInterval(mean, stddev, floor(mean - 1.0));
    if (maxValue < cur) maxValue = cur;
    if (configVerbose > 3) fprintf(stderr, "(m-1, m): %.17g\n", cur);
  }

  // Those things digitizing to m
  //(m, m+1)
  if ((mean >= 0.0) && (mean + 1.0 <= max)) {
    cur = integrateIntegerInterval(mean, stddev, floor(mean));
    if (maxValue < cur) maxValue = cur;
    if (configVerbose > 3) fprintf(stderr, "(m, m+1): %.17g\n", cur);
  }

  // Those things digitizing to m+1
  //(m+1, m+2)
  if ((mean + 1.0 >= 0.0) && (mean + 2.0 <= max)) {
    cur = integrateIntegerInterval(mean, stddev, floor(mean + 1.0));
    if (maxValue < cur) maxValue = cur;
    if (configVerbose > 3) fprintf(stderr, "(m+1, m+2): %.17g\n", cur);
  }

  curMinEnt = -log(maxValue) / log(2.0);

  assert(curMinEnt >= 0.0);
  assert(curMinEnt <= bits);

  return curMinEnt;
}

/*This produces an uint32_t integer that is a discrete sampling of a truncated normal distribution with the
 *stated mean and stddev described here*/
/*This models a discrete ADC sampling a normal process*/
uint32_t genNormalInt(double mean, double stddev, uint32_t bits, struct sinBiasState *bstate, struct randstate *rstate) {
  int64_t randomOut = -1;
  double dout1;
  uint32_t max;
  double discard;
  double curBias;

  assert(rstate != NULL);
  assert(rstate->seeded);
  assert(stddev >= 0.0);
  assert(bits < 32);

  max = ((1U << bits) - 1);
  assert((uint32_t)mean + 2 < max);

  normalVariate(mean, stddev, &dout1, NULL, rstate);
  if (configVerbose > 5) {
    fprintf(stderr, "normal 1: %.17g\n", dout1);
  }

  if (bstate != NULL) {
    bstate->currentPhase = modf(bstate->currentPhase + 1.0 / bstate->period, &discard);
    curBias = bstate->magnitude * sin(bstate->currentPhase * (2 * M_PI));
    dout1 += curBias;
    bstate->lastEntropy = truncatedNormalminEnt(mean + curBias, stddev, bits);
  }

  if (dout1 < 0.0) {
    randomOut = 0;
  } else if (dout1 > max) {
    randomOut = max;
  } else {
    randomOut = (int64_t)dout1;
  }

  return (uint32_t)randomOut;
}

unsigned char *genRandBitBytes(double bias, size_t datalen, struct randstate *rstate) {
  size_t i;
  size_t zeroCount = 0;
  unsigned char *data;

  assert((bias >= -1) && (bias <= 1));

  if ((data = malloc(datalen * sizeof(unsigned char))) == NULL) {
    perror("Can't allocate memory for randomly generated data for review.");
    exit(EX_OSERR);
  }

  for (i = 0; i < datalen; i++) {
    if ((data[i] = (unsigned char)genRandBiasedBit(bias, rstate)) == 0) {
      zeroCount++;
    }
  }

  if (configVerbose > 1) {
    fprintf(stderr, "Generated %zu 0s of %zu bits\n", zeroCount, datalen);
  }

  return (data);
}

void genRandInts(statData_t *data, size_t datalen, uint32_t k, struct randstate *rstate) {
  size_t i;

  assert(rstate != NULL);
  assert(rstate->seeded);
  assert(k <= STATDATA_MAX);

  for (i = 0; i < datalen; i++) {
    data[i] = (statData_t)randomRange(k, rstate);
  }
}

/*Produces a double with the full significand worth of randomness, where "adjacent" possible values are evenly spaced, in the interval [0,1)*/
inline double randomUnit(struct randstate *rstate) {
  /*Note that 2^53 is the largest integer that can be represented in a 64 bit IEEE 754 double, such that all smaller positive integers can also be
   * represented. Anding by 0x001FFFFFFFFFFFFFULL masks out the lower 53 bits, so the resulting integer is in the range [0, 2^53 - 1].
   * 0x1.0p-53 is 2^(-53) (multiplying by this value just effects the exponent of the resulting double, not the significand)!
   * We get a double uniformly distributed in the range [0, 1).  (The delta between adjacent values is 2^(-53))
   * There is a lot of BS on the internet on how to do this well. This is the best I could come up with, and
   * it appears to be the standard approach (used by the corrected SFMT and xoroshiro authors).
   */
  return (((double)(randomu64(rstate) & 0x001FFFFFFFFFFFFFULL)) * 0x1.0p-53);
  // out is now in [0, 1)
}

/*Produces a double with the full significand worth of randomness, where "adjacent" possible values are evenly spaced, in the interval (-1,1)*/
double randomSignedUnit(struct randstate *rstate) {
  uint64_t in;
  double out;

  in = randomu64(rstate);
  /*Note that 2^53 is the largest integer that can be represented in a 64 bit IEEE 754 double, such that all smaller positive integers can also be
   * represented. Anding by 0x001FFFFFFFFFFFFFULL masks out the lower 53 bits, so the resulting integer is in the range [0, 2^53 - 1].
   * 0x1.0p-53 is 2^(-53) (multiplying by this value just effects the exponent of the resulting double, not the significand)!
   * We get a double uniformly distributed in the range [0, 1).  (The delta between adjacent values is 2^(-53))
   * There is a lot of BS on the internet on how to do this well. This is the best I could come up with, and
   * it appears to be the standard approach (used by the corrected SFMT and xoroshiro authors).
   */
  out = ((double)(in & 0x001FFFFFFFFFFFFFULL)) * 0x1.0p-53;
  // out is now in [0, 1)

  // We'll treat the LSb as the sign bit; this bit is necessarily discarded when creating "out" above.
  if ((in & 0x0020000000000000ULL) != 0) {
    // The sign bit is set, so we should generally made the output negative, unless out is identically 0.0
    // If the output is equal to 0.0, this makes 0 equally likely, as 0 == -0 (though they have different representations)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (likely(out != 0.0)) {
#pragma GCC diagnostic pop
      out *= -1.0;
    } else {
      /*In the unlikely event that we generated exactly 0, and want to make it negative, try again. OW 0 is twice as likely as everything else*/
      return randomSignedUnit(rstate);
    }
  }

  // out is now in (-1, 1)
  return out;
}

/*Uses the Marsaglia polar method*/
/*https://en.wikipedia.org/wiki/Marsaglia_polar_method*/
void normalVariate(double mean, double stdDev, double *out1, double *out2, struct randstate *rstate) {
  double x = 1.0;
  double y = 1.0;
  double s = 1.0;
  double sfactor;

  assert(out1 != NULL);
  assert(rstate != NULL);
  assert(rstate->seeded);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  while ((s >= 1.0) || (s == 0.0)) {
#pragma GCC diagnostic pop
    x = randomSignedUnit(rstate);
    y = randomSignedUnit(rstate);

    if (configVerbose > 8) {
      fprintf(stderr, "x: %.17g, y: %.17g\n", x, y);
    }
    s = x * x + y * y;
  }

  sfactor = sqrt(-2.0 * log(s) / s);
  if (configVerbose > 8) {
    fprintf(stderr, "s: %.17g, sfactor: %.17g\n", s, sfactor);
  }

  *out1 = x * sfactor * stdDev + mean;
  if (out2 != NULL) {
    *out2 = y * sfactor * stdDev + mean;
  }
}

// Step Update Metastable Source (SUMS), as per the CRI report and DJ Johnston's implementation
/*Following the logic presented in the CRI report (page 13)*/
uint32_t SUMSgenerate(struct SUMSstate *sstate, struct randstate *rstate) {
  double currentSimulationRand;
  double baseStepNoise;
  double currentValue;
  double prOne;

  assert(sstate != NULL);
  assert(rstate != NULL);

  // This round's value for comparison; account for serial Adjustment here
  currentValue = sstate->bias + sstate->serialCoefficient * (((double)(sstate->lastOutput)) * 2.0 - 1.0);

  // What is the chance that currentValue > X?
  // As X is taken from a standard normal distribution, this is \int_{currentValue}^{\infty} {PDF(x) dx}, which is 1/2 Erfc[currentValue/Sqrt[2]]
  prOne = 1.0 - normalCDF(currentValue, 0.0, 1.0);
  assert((prOne >= 0.0) && (prOne <= 1.0));

  // Calculate the min entropy associated with this round
  sstate->lastEntropy = -log(fmax(prOne, 1.0 - prOne)) / log(2.0);

  // This round's Simulation value
  normalVariate(0.0, 1.0, &currentSimulationRand, &baseStepNoise, rstate);

  if (currentValue > currentSimulationRand) {
    sstate->lastOutput = 1;
    sstate->bias -= sstate->leftStepSize;
  } else {
    sstate->lastOutput = 0;
    sstate->bias += sstate->rightStepSize;
  }

  // Step noise
  // baseStepNoise is with respect a standard normal distribution, so adjust it, and then add it to the bias.
  // this can be disabled by setting the mean and standard deviation to 0.
  sstate->bias += (baseStepNoise * sstate->stepNoiseStdDev + sstate->stepNoiseMean);

  return (sstate->lastOutput);
}

/*The probability of outputting a 1 is p.*/
uint8_t genRandBit(double p, struct randstate *rstate) {
  return (randomUnit(rstate) < p) ? 1U : 0U;
}

uint32_t genRandCorrelatedBit(double c, uint32_t lastbit, struct randstate *rstate) {
  if (randomUnit(rstate) < (c + 1.0) / 2.0)
    return lastbit;
  else
    return (lastbit == 0) ? 1 : 0;
}

/*A source of polynomials: https://users.ece.cmu.edu/~koopman/lfsr/index.html*/
static bool lfsrInc(uint64_t *state, uint64_t polynomial) {
  uint64_t lsb;

  lsb = *state & 1;
  *state >>= 1;
  if (lsb == 1) {
    *state ^= polynomial;
  }
  return (lsb == 1);
}

/*This produces an uint32_t integer that is the result of incrementing a LFSR, incremented a number of times. The number of increments is a discrete sampling of a truncated normal distribution with the
 *stated mean and stddev described here*/
uint32_t LFSRFilter(uint32_t input, uint32_t bits, uint64_t *state, uint64_t polynomial) {
  uint32_t randomOut = 0;
  uint32_t j;

  assert(bits < 32);
  assert(input >= bits);

  for (j = 0; j < (input - bits); j++) {
    lfsrInc(state, polynomial);
  }

  for (j = 0; j < bits; j++) {
    randomOut <<= 1;
    if (lfsrInc(state, polynomial)) randomOut |= 1U;
  }

  return randomOut;
}

/*This is the Fisher Yates shuffle*/
/*See https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle*/
void FYShuffle(double *data, size_t datalen, struct randstate *rstate) {
  size_t i, j;
  double tmp;

  assert(datalen > 0);
  assert(datalen <= UINT32_MAX);

  for (i = datalen - 1; i >= 1; i--) {
    j = randomRange((uint32_t)i, rstate);
    tmp = data[i];
    data[i] = data[j];
    data[j] = tmp;
  }
}

/*Convention: Sampling when RO phase < 0.5 yields a 0, and sampling when RO phase >= 0.4 yields a 1*/
statData_t ringOscillatorNextDeterministicSample(double oscFreq, double oscPhase, double sampleFreq, double samplePhase) {
  double samplePeriod;
  double oscillatorPeriod;
  double ipart;
  double detOscPhase;
  double endPhase;

  assert(oscFreq > 0);
  assert(sampleFreq > 0);
  assert(sampleFreq < oscFreq);
  assert((oscPhase >= 0) && (oscPhase < 1.0));
  assert((samplePhase >= 0) && (samplePhase < 1.0));

  samplePeriod = 1.0 / sampleFreq;
  oscillatorPeriod = 1.0 / oscFreq;

  endPhase = ((1.0 - samplePhase) * samplePeriod - (1.0 - oscPhase) * oscillatorPeriod) / oscillatorPeriod;
  if (configVerbose > 5) {
    fprintf(stderr, "Det got end osc phase %.17g\n", endPhase);
  }
  detOscPhase = modf(endPhase, &ipart);

  assert((detOscPhase < 1.0) && (detOscPhase >= 0.0));

  return ((detOscPhase < 0.5) ? 0 : 1);
}

/*The probability of moving above NORMALOUTLER = 7.0477*sigma is less than 2^(-40)*/
/*The probability of moving above 4.763*sigma is less than 2^(-20)*/
/*We try to package as many of these as will likely fit*/
/*We take increasingly smaller jumps, until we (with high probability) overshoot the sample by 1*/
/*When that happens, we know what the sample state was (the prior state)*/
/*and we know what the new sample phase is (the sample clock's phase after the current transition*/
/*All calculations here are done with respect to the transition period, not the RO period*/
static uint64_t packageCycles(double target, double *start, double mean, double stddev, struct randstate *rstate) {
  uint64_t cycles;
  double k;
  double curNormal, nextNormal;
  double delta;
  double current;
  double currentAdjustment;
  double maxAdjustment;

  assert(rstate != NULL);
  assert(mean > 0.0);
  assert(stddev >= 0.0);
  assert(start != NULL);
  assert(target >= *start);

  nextNormal = DBL_INFINITY;
  cycles = 0;
  current = *start;
  k = 0;  // retain the bundle size (number of transitions we are trying to simulate at once) in k

  // the target is the next transition.
  while (current < target) {
    delta = target - current;
    assert(delta > 0.0);
    // k is the number of values we can bundle together with non-negligible probability of overshooting.
    if (stddev > 0.0) {
      // Calculate k using the quadratic formula
      k = positiveStrictFloor(square((sqrt(square(NORMALOUTLIER * stddev) + 4.0 * mean * delta) - NORMALOUTLIER * stddev) / (2.0 * mean)));
    } else {
      /*This branch shouldn't be necessary (it's mathematically equivalent to the above case when stddev == 0),
       *but it seemed that floating point error would sometimes bump the value up above the allowed value.
       */
      k = positiveStrictFloor(delta / mean);
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    // k should be an integer value, in the range [0, INTDOUBLE_MAX]
    assert((floor(k) == k) && (k >= 0.0) && (k <= INTDOUBLE_MAX));
#pragma GCC diagnostic pop

    if ((k * mean + NORMALOUTLIER * sqrt(k) * stddev >= delta)) {
      fprintf(stderr, "Overshot when calculating k for packaging: mean: %.17g, stddev: %1.17g, Bundle size: k=%.17g, NORMALOUTLIER: %.17g, likely bound = %.17g >= %.17g = delta\n", mean, stddev, k, NORMALOUTLIER, k * mean + NORMALOUTLIER * sqrt(k) * stddev,
              delta);
      // Note, k is integer and delta > 0, so if we're here, k >= 1.0
      assert(k >= 1.0);
      k = k - 1.0;
    } else if (configVerbose > 5) {
      fprintf(stderr, "Calculating k: mean: %.17g, stddev: %1.17g, Bundle size: k=%.17g, likely bound =  %.17g < %.17g = delta\n", mean, stddev, k, k * mean + NORMALOUTLIER * sqrt(k) * stddev, delta);
    }

    assert((k * mean + NORMALOUTLIER * sqrt(k) * stddev) < delta);

    // You always need to try at least 1 at a time.
    if (k < 1.0) {
      k = 1.0;
    }

    // k should be an integer value, in the range [1, INTDOUBLE_MAX]
    if (configVerbose > 6) {
      fprintf(stderr, "doing a %.17g-package\n", k);
    }

    // Use the extra normal value if there is one
    if (nextNormal < DBL_INFINITY) {
      curNormal = nextNormal;
      nextNormal = DBL_INFINITY;
    } else {
      // There wasn't one. Generate a new pair of standard normal values
      normalVariate(0.0, 1.0, &curNormal, &nextNormal, rstate);
    }

    currentAdjustment = curNormal * (sqrt(k) * stddev) + k * mean;

    maxAdjustment = 2.0 * target - current;

    // We can't tolerate time travel
    if ((currentAdjustment > 0.0) && (currentAdjustment < maxAdjustment)) {
      current += curNormal * (sqrt(k) * stddev) + k * mean;
      cycles += (uint64_t)k;
    } else {
      if ((configVerbose > 1) && (currentAdjustment <= 0.0)) {
        fprintf(stderr, "Killed a sad-faced time traveler.\n");
      } else if ((configVerbose > 1) && (currentAdjustment >= maxAdjustment)) {
        fprintf(stderr, "Killed a happy-faced overachiever.\n");
      } else {
        fprintf(stderr, "I killed someone just to watch them die.\n");
      }
    }
  }

  if (k > 1.0) {
    // This is wildly unlikely (pr < 2^-40)
    if (configVerbose > 1) {
      fprintf(stderr, "Overshot. Retrying.\n");
    }
    return (packageCycles(target, start, mean, stddev, rstate));
  } else {
    // We overshot by exactly one, so we know how many transitions occurred
    *start = current;
    return (cycles);
  }
}

// relative phases are values in the range [0, 1)
// initOscPhase is expressed as time values in the range [0,oscillatorPeriod/2)
/*As a convention, we perform the simulation where each simulation round (other than the very first
 *starts at a RO transition. We adjust the sampling clock phase to account for the slop between samples*/
statData_t ringOscillatorNextNonDeterministicSample(double oscFreq, double oscJitter, double *relOscPhase, double sampleFreq, double *relSamplePhase, struct randstate *rstate) {
  double samplePeriod;
  double transitionOscJitter;
  double oscillatorPeriod;
  uint64_t nonDetCycles;  // the number of transitions that it takes to overshoot of the sampling period.
  uint64_t outputCorrection;
  statData_t nonDetOutput;
  double curTransition;
  double accruedTime;
  double initOscPhase;
  double oscillatorTransition;
  double maxTransition;

  assert(oscFreq > 0);
  assert(oscJitter >= 0);
  assert(sampleFreq > 0);
  assert(oscFreq > sampleFreq);
  assert(relOscPhase != NULL);
  assert((*relOscPhase >= 0) && (*relOscPhase < 1.0));
  assert(relSamplePhase != NULL);
  assert((*relSamplePhase >= 0) && (*relSamplePhase < 1.0));

  samplePeriod = 1.0 / sampleFreq;
  oscillatorPeriod = 1.0 / oscFreq;
  oscillatorTransition = oscillatorPeriod / 2.0;
  transitionOscJitter = oscJitter / sqrt(2);

  // In this logic, the literal values 0.0 and 0.5 are set by other logic
  // Any other value is only due to this being the very first simulation round
  if (*relOscPhase >= 0.5) {
    /*If the phase is .5 or later, this corresponds to a 1*/
    // Multiplication by exactly 0.0 will yield exactly 0.0
    initOscPhase = (*relOscPhase - 0.5) * oscillatorPeriod;
    outputCorrection = 1;
  } else {
    /*If the phase is less than .5, this corresponds to a 0*/
    // Multiplication by exactly 0.0 will yield exactly 0.0
    initOscPhase = (*relOscPhase) * oscillatorPeriod;
    outputCorrection = 0;
  }

  assert(initOscPhase < 0.5 * oscillatorPeriod);

  /*This is the starting condition from any prior simulation round*/
  accruedTime = *relSamplePhase * samplePeriod;

  assert((initOscPhase >= 0) && (initOscPhase < oscillatorTransition));
  assert((accruedTime >= 0) && (accruedTime < samplePeriod));

  /*Now do the simulation*/
  /*First, account for any initial over oscillator phase*/
  if (initOscPhase > 0.0) {
    /*Here, we have slop, so our first oscillator cycle was partially consumed by the last sample cycle*/
    curTransition = -1.0;
    maxTransition = 2 * samplePeriod - accruedTime;
    while ((curTransition <= 0.0) || (curTransition >= maxTransition)) {
      normalVariate(oscillatorTransition, transitionOscJitter, &curTransition, NULL, rstate);
      if (curTransition <= 0.0) {
        if (configVerbose > 1) fprintf(stderr, "Killed a pesky time traveler.\n");
      } else if (curTransition >= maxTransition) {
        if (configVerbose > 1) fprintf(stderr, "Killed a pesky overachiever.\n");
      }
    }

    accruedTime += curTransition;
    nonDetCycles = 1;
    if (configVerbose > 5) {
      fprintf(stderr, "Initial cycle processing resulted in and adjustment of the current time by %.17g\n", curTransition);
    }
  } else {
    nonDetCycles = 0;
  }

  if (accruedTime < samplePeriod) {
    /*When the accumulated oscCycles exceed the sample period, then a sample occurred on the prior cycle*/
    nonDetCycles += packageCycles(samplePeriod, &accruedTime, oscillatorTransition, transitionOscJitter, rstate);
  }

  /*The current cycle represented in nonDetCycles reflects the cycle after we sampled.
   *accruedTime is be beyond that sample time.*/
  if (configVerbose > 5) {
    fprintf(stderr, "oscCycles %lu\n", nonDetCycles);
  }

  /*nonDetCycles is now calculated. A value "1" here would imply that no RO
   *transition occurred prior to sampling, and the initial state is the sampled state
   *Thus the number of transitions is nonDetCycles - 1.
   */

  assert(nonDetCycles > 0);
  nonDetOutput = (nonDetCycles - 1 + outputCorrection) & 0x01;
  if (configVerbose > 5) {
    fprintf(stderr, "Final output: %u\n", nonDetOutput);
  }

  /*In the simulation, the RO transitioned after sampling, so the current phase isn't in the phase region
   *corresponding to the sampled value.
   *That is, if nonDetOutput is 0, corresponding to a RO phase when sampled in the range [0,.5),
   *then we set the next RO phase to 0.5.
   *Similarly, if nonDetOutput is 1, corresponding to a RO phase when sampled in the range [0.5,1),
   *then we set the next RO phase to 0.0.
   *Thus, other than the first sample, the ring oscillator phase is either 0.0 or 0.5,
   *as each simulation round ends at a RO transition.
   */
  *relOscPhase = (nonDetOutput == 0) ? 0.5 : 0.0;
  if (configVerbose > 5) {
    fprintf(stderr, "Next osc phase: %.17g\n", *relOscPhase);
  }

  /*The sample phase varies. We need to account for the extra time taken
   *after the last RO transition, so that the sample clock phase is correct for the next round.
   */
  assert((accruedTime >= samplePeriod) && (accruedTime < 2.0 * samplePeriod));
  *relSamplePhase = (accruedTime - samplePeriod) / samplePeriod;
  if (configVerbose > 5) {
    fprintf(stderr, "Next sample phase: %.17g\n", *relSamplePhase);
  }
  assert((*relSamplePhase >= 0.0) && (*relSamplePhase < 1.0));

  return nonDetOutput;
}
