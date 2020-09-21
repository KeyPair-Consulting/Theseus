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
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <tgmath.h>

#include "entlib.h"
#include "fancymath.h"
#include "incbeta.h"

#ifdef SLOWCHECKS
#include "cephes.h"
#endif

// This generally performs a check for relative closeness, but (if that check would be nonsense)
// it can check for an absolute separation, using either the distance between the numbers, or
// the number of ULPs that separate the two numbers.
// See the following for details and discussion of this approach:
// https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
// https://floating-point-gui.de/errors/comparison/
// https://www.boost.org/doc/libs/1_62_0/libs/test/doc/html/boost_test/testing_tools/extended_comparison/floating_point/floating_points_comparison_theory.html
// Knuth AoCP vol II (section 4.2.2)
// Tested using modified test cases from https://floating-point-gui.de/errors/NearlyEqualsTest.java
bool relEpsilonEqual(double A, double B, double maxAbsFactor, double maxRelFactor, uint64_t maxULP) {
  double diff;
  double absA, absB;
  uint64_t Aint;
  uint64_t Bint;
  uint64_t ULPdiff;

  assert(sizeof(uint64_t) == sizeof(double));
  assert(maxAbsFactor >= 0.0);
  assert(maxRelFactor >= 0.0);

  /// NaN is by definition not equal to anything (including itself)
  if (isnan(A) || isnan(B)) {
    if (configVerbose > 4) fprintf(stderr, "One or both values are NaN, which are never equal to anything.\n");
    return false;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  // Deals with equal infinities, and the corner case where they are actually copies
  if (A == B) {
    if (configVerbose > 4) fprintf(stderr, "The values are literally equal.\n");
    return true;
  }
#pragma GCC diagnostic pop

  // If either is infinity, but they are not equal, then they aren't close.
  if (isinf(A) || isinf(B)) {
    if (configVerbose > 4) fprintf(stderr, "At least one of the values are infinite (but not the same one!).\n");
    return false;
  }

  absA = fabs(A);
  absB = fabs(B);
  // Make sure that A is the closest to 0.
  if (absA > absB) {
    double tmp;

    // Swap A and B
    tmp = B;
    B = A;
    A = tmp;

    // Swap absA and absB
    tmp = absB;
    absB = absA;
    absA = tmp;
  }

  // Capture the difference of the largest magnitude from the smallest magnitude
  diff = fabs(B - A);

  // Is absA, diff, or absB * maxRelFactor subnormal?
  // Did diff overflow?
  // if absA is subnormal (effectively 0) or 0, then relative difference isn't meaningful, as fabs(B-A)/B≈1 for all values of B
  // In the instance of overflows, the resulting relative comparison will be nonsense.
  if ((absA < DBL_MIN) || (diff < DBL_MIN) || isinf(diff) || (absB * maxRelFactor < DBL_MIN)) {
    // Yes. Relative closeness is going to be nonsense
    if (configVerbose > 4) fprintf(stderr, "Absolute diff: %.17g, maxAbsFactor: %.17g\n", diff, maxAbsFactor);
    return diff <= maxAbsFactor;
  } else {
    // No. Using relative closeness is probably the right thing to do.
    // Proceeding roughly as per Knuth AoCP vol II (section 4.2.2)
    if (configVerbose > 4) fprintf(stderr, "Relative diff / absB: %.17g, relEp: %.17g\n", diff / absB, maxRelFactor);
    if (diff <= absB * maxRelFactor) {
      // These are relatively close
      return true;
    }
  }

  // Neither A or B is subnormal, and they aren't close in the conventional sense,
  // but perhaps that's just due to IEEE representation. Check to see if the value is within maxULP ULPs.

  // We can't meaningfully compare non-zero values with 0.0 in this way,
  // but absA >= DBL_MIN if we're here, so neither value is 0.0.

  // if they aren't the same sign, then these can't be only a few ULPs away from each other
  if (signbit(A) != signbit(B)) {
    if (configVerbose > 4) fprintf(stderr, "The values are not relatively or absolutely close, and are of different signs.\n");
    return false;
  }

  // Note, casting from one type to another is undefined behavior, but memcpy will necessarily work
  memcpy(&Aint, &absA, sizeof(double));
  memcpy(&Bint, &absB, sizeof(double));
  // This should be true by the construction of IEEE doubles
  assert(Bint > Aint);

  ULPdiff = Bint - Aint;
  if (configVerbose > 4) fprintf(stderr, "Exactly %zu ULPs separate the values\n", ULPdiff);
  return (ULPdiff <= maxULP);
}

bool relEpsilonEquall(long double A, long double B, long double maxAbsFactor, long double maxRelFactor, uint64_t maxULP) {
  long double diff;
  long double absA, absB;
  long double maxULPdelta;

  assert(maxAbsFactor >= 0.0L);
  assert(maxRelFactor >= 0.0L);

  /// NaN is by definition not equal to anything (including itself)
  if (isnan(A) || isnan(B)) {
    if (configVerbose > 4) fprintf(stderr, "One or both values are NaN, which are never equal to anything.\n");
    return false;
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  // Deals with equal infinities, and the corner case where they are actually copies
  if (A == B) {
    if (configVerbose > 4) fprintf(stderr, "The values are literally equal.\n");
    return true;
  }
#pragma GCC diagnostic pop

  // If either is infinity, but they are not equal, then they aren't close.
  if (isinf(A) || isinf(B)) {
    if (configVerbose > 4) fprintf(stderr, "At least one of the values are infinite (but not the same one!).\n");
    return false;
  }

  absA = fabsl(A);
  absB = fabsl(B);
  // Make sure that A is the closest to 0.
  if (absA > absB) {
    long double tmp;

    // Swap A and B
    tmp = B;
    B = A;
    A = tmp;

    // Swap absA and absB
    tmp = absB;
    absB = absA;
    absA = tmp;
  }

  // Capture the difference of the largest magnitude from the smallest magnitude
  diff = fabsl(B - A);

  // Is absA, diff, or absB * maxRelFactor subnormal?
  // Did diff overflow?
  // if absA is subnormal (effectively 0) or 0, then relative difference isn't meaningful, as fabs(B-A)/B≈1 for all values of B
  // In the instance of overflows, the resulting relative comparison will be nonsense.
  if ((absA < LDBL_MIN) || (diff < LDBL_MIN) || isinf(diff) || (absB * maxRelFactor < LDBL_MIN)) {
    // Yes. Relative closeness is going to be nonsense
    if (configVerbose > 4) fprintf(stderr, "Absolute diff: %.22Lg, maxAbsFactor: %.22Lg\n", diff, maxAbsFactor);
    return (diff <= maxAbsFactor);
  } else {
    // No. Using relative closeness is probably the right thing to do.
    // Proceeding roughly as per Knuth AoCP vol II (section 4.2.2)
    if (configVerbose > 4) fprintf(stderr, "Relative diff / absB: %.22Lg, relEp: %.22Lg\n", diff / absB, maxRelFactor);
    if (diff <= absB * maxRelFactor) {
      // These are relatively close
      return true;
    }
  }

  // Neither A or B is subnormal, and they aren't close in the conventional sense,
  // but perhaps that's just due to IEEE representation. Check to see if the value is within maxULP ULPs.

  // We can't meaningfully compare non-zero values with 0.0 in this way,
  // but absA >= DBL_MIN if we're here, so neither value is 0.0.

  // if they aren't the same sign, then these can't be only a few ULPs away from each other
  if (signbit(A) != signbit(B)) {
    if (configVerbose > 4) fprintf(stderr, "The values are not relatively or absolutely close, and are of different signs.\n");
    return false;
  }

  // an ULP is going to be largest when furthest away from zero.
  maxULPdelta = fabsl(absB - nextafterl(absB, absA));
  assert(maxULPdelta >= 0.0L);

  if (maxULPdelta > 0.0L) {
    long double approxULPdelta = ceill(diff / maxULPdelta);
    if (approxULPdelta < (long double)UINT64_MAX) {
      if (configVerbose > 4) fprintf(stderr, "Approximately %zu ULPs separate the values\n", (uint64_t)approxULPdelta);
      return (maxULP >= (uint64_t)approxULPdelta);
    } else {
      if (configVerbose > 4) fprintf(stderr, "It appears that many ULPs separate the values\n");
      return false;
    }
  } else {
    if (configVerbose > 4) fprintf(stderr, "It appears that 0 ULPs separate the values\n");
    // apparently, absB == nextafterl(absB, absA)
    return true;
  }
}

static void initKahanSum(/*@out@*/ struct kahanState *state) {
  state->sum = 0.0;
  state->compensation = 0.0;
}

static void kahanSum(struct kahanState *state, double summand) {
  KAHANACCUMULATOR y, t;

  assert(state != NULL);

  y = summand - state->compensation;
  t = state->sum + y;  // t = sum' + summand - compensation'
  state->compensation = (t - state->sum) - y;  // comp = ((sum' + summand - compensation') - sum') - (summand - compensation') = 0.0 (under perfect rounding)
  state->sum = t;  // sum = sum' + summand - compensation'
}

// state1 += state2
static void kahanAdd(struct kahanState *state1, struct kahanState *state2, double scalar) {
  KAHANACCUMULATOR y, t;

  assert(state1 != NULL);
  assert(state2 != NULL);

  // First add in the state2 compensation
  y = scalar * state2->compensation - state1->compensation;
  t = state1->sum + y;
  state1->compensation = (t - state1->sum) - y;
  state1->sum = t;

  // Next add in the state2 sum
  y = scalar * state2->sum - state1->compensation;
  t = state1->sum + y;
  state1->compensation = (t - state1->sum) - y;
  state1->sum = t;
}

static double kahanSumResult(struct kahanState *state) {
  return (double)state->sum;
}

static long double kahanSumResultl(struct kahanState *state) {
  long double result;
  result = (long double)state->sum + (long double)state->compensation;
  return result;
}

// As per
// Adaptive Precision Floating-Point Arithmetic and Fast Robust Geometric Predicates
// Jonathan Richard Shewchuk
// http://www-2.cs.cmu.edu/afs/cs/project/quake/public/papers/robust-arithmetic.ps
// Generally adapted after the Python equivalent function msum / fsum
// https://github.com/python/cpython/blob/master/Modules/mathmodule.c
static void initAdaptiveSum(struct adaptiveState *state) {
  if ((state->partials = malloc(PARTIALBLOCK * sizeof(double))) == NULL) {
    perror("Can't allocate partials list");
    exit(EX_OSERR);
  }

  state->numOfPartials = 0;
  state->partialListCapacity = PARTIALBLOCK;
}

static void growAdaptiveSum(struct adaptiveState *state) {
  state->partialListCapacity += PARTIALBLOCK;
  if ((state->partials = realloc(state->partials, state->partialListCapacity * sizeof(double))) == NULL) {
    perror("Can't allocate partials list");
    exit(EX_OSERR);
  }
}

static void adaptiveSum(struct adaptiveState *state, double x) {
  size_t i, j;
  double y;
  double tmp, low, high;

  assert(state != NULL);
  assert(state->partials != NULL);

  i = 0;
  for (j = 0; j < state->numOfPartials; j++) {
    y = state->partials[j];
    if (fabs(x) < fabs(y)) {
      tmp = x;
      x = y;
      y = tmp;
    }

    high = x + y;
    low = y - (high - x);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (low != 0.0) {
      assert(i < state->partialListCapacity);
      state->partials[i] = low;
      i++;
    }
#pragma GCC diagnostic pop
    x = high;
  }

  state->numOfPartials = i + 1;
  if (state->numOfPartials > state->partialListCapacity) growAdaptiveSum(state);
  state->partials[i] = x;
}

// state1 += state2
static void adaptiveAdd(struct adaptiveState *state1, struct adaptiveState *state2, double scalar) {
  size_t i;
  assert(state1 != NULL);
  assert(state2 != NULL);

  for (i = 0; i < state2->numOfPartials; i++) {
    adaptiveSum(state1, scalar * state2->partials[i]);
  }
}

static long double adaptiveSumResultl(struct adaptiveState *state) {
  long double sum = 0.0L;

  assert(state != NULL);
  assert(state->numOfPartials <= SSIZE_MAX);

  if (configVerbose > 6) fprintf(stderr, "Sum over %zu partial sums\n", state->numOfPartials);
  if (state->numOfPartials == 0) return (0.0L);

  // This doesn't account for the necessary rounding, but it's at most off by whatever the lsb of the long double represents
  for (ssize_t j = ((ssize_t)(state->numOfPartials)) - 1; j >= 0; j--) {
    sum += (long double)(state->partials[j]);
  }

  return sum;
}

static double adaptiveSumResult(struct adaptiveState *state) {
  double sumLow;
  double sumHigh;
  double x;
  double y;
  double yr;

  ssize_t j;

  assert(state != NULL);
  assert(state->numOfPartials <= SSIZE_MAX);

  if (configVerbose > 6) fprintf(stderr, "Sum over %zu partial sums\n", state->numOfPartials);
  if (state->numOfPartials == 0) return (0.0);

  sumLow = 0.0;
  sumHigh = state->partials[state->numOfPartials - 1];

  for (j = ((ssize_t)(state->numOfPartials)) - 2; j >= 0; j--) {
    x = sumHigh;
    y = state->partials[j];
    assert(fabs(y) < fabs(x));

    // If no errors:
    sumHigh = x + y;  // sumHigh[k] = sumHigh[k-1] + state->partials[j]
    yr = sumHigh - x;  // yr = sumHigh[k-1] + state->partials[j] - sumHigh[k-1]
    sumLow = y - yr;  // sumLow = state->partials[j] - state->partials[j]

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (sumLow != 0.0) {
      break;
    }
#pragma GCC diagnostic pop
  }

  // The result is now in sumHigh, sumLow

  // This accounts for any needed rounding
  // See https://bugs.python.org/file10357/msum4.py for proofs
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  if ((sumLow != 0.0) && (j >= 1) && (signbit(sumLow) == signbit(state->partials[j - 1]))) {
    y = sumLow * 2.0;
    x = sumHigh + y;
    yr = x - sumHigh;
    if (y == yr) {
      // We have just determined that partials[-1] + 2*partials[-2] is exactly representable
      // we should round in this instance.
      sumHigh = x;
    }
#pragma GCC diagnostic pop
  }

  return sumHigh;
}

static void delAdaptiveSum(struct adaptiveState *state) {
  assert(state != NULL);
  assert(state->partials != NULL);
  free(state->partials);
  state->partials = NULL;
  state->numOfPartials = 0;
  state->partialListCapacity = 0;
}

// For comparing results between summation approaches.
static void compareSumResults(double adaptiveRes, double kahanRes, double rawRes) {
  fprintf(stderr, "Finalizing sum result: %.17g. Rel errors: kahan: %.17g raw: %.17g\n", adaptiveRes, RELFACTOR(adaptiveRes, kahanRes), RELFACTOR(adaptiveRes, rawRes));
}

void initCompensatedSum(struct compensatedState *state, const char *label, size_t errorIndex) {
  assert(state != NULL);
  if (configVerbose > 2) {
    initKahanSum(&(state->kState));
  }
  initAdaptiveSum(&(state->aState));
  state->naiveSum = 0.0;

  strncpy(state->label, label, LABELLEN - 1);
  state->label[LABELLEN - 1] = '\0';
  state->errorIndex = errorIndex;
}

void delCompensatedSum(struct compensatedState *state) {
  delAdaptiveSum(&(state->aState));
  state->naiveSum = 0.0;
}

long double compensatedSumResultl(struct compensatedState *state) {
  long double aRes, kRes;
  long double kError, nError;

  assert(state != NULL);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  aRes = adaptiveSumResultl(&(state->aState));

  if (configVerbose > 2) {
    kRes = kahanSumResultl(&(state->kState));
    kError = RELFACTORL(aRes, kRes);
    nError = RELFACTORL(aRes, (long double)state->naiveSum);

    if ((double)kError > globalErrors[2 * state->errorIndex]) {
      strncpy(errorLabels[2 * state->errorIndex], state->label, LABELLEN - 1);
      errorLabels[2 * state->errorIndex][LABELLEN - 1] = '\0';
      strncat(errorLabels[2 * state->errorIndex], " kahan", LABELLEN - 1);

      globalErrors[2 * state->errorIndex] = (double)kError;
    }

    if ((double)nError > globalErrors[2 * state->errorIndex + 1]) {
      strncpy(errorLabels[2 * state->errorIndex + 1], state->label, LABELLEN - 1);
      errorLabels[2 * state->errorIndex + 1][LABELLEN - 1] = '\0';
      strncat(errorLabels[2 * state->errorIndex + 1], " naive", LABELLEN - 1);

      globalErrors[2 * state->errorIndex + 1] = (double)nError;
    }

    if ((configVerbose > 6) || (RELFACTORL(aRes, kRes) > (long double)DBL_EPSILON)) {
      fprintf(stderr, "%s: ", state->label);
      compareSumResults((double)aRes, (double)kRes, state->naiveSum);
    }
  }

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);

  return (aRes);
}

double compensatedSumResult(struct compensatedState *state) {
  double aRes, kRes;
  double kError, nError;

  assert(state != NULL);

  aRes = adaptiveSumResult(&(state->aState));

  if (configVerbose > 2) {
    kRes = kahanSumResult(&(state->kState));
    kError = RELFACTOR(aRes, kRes);
    nError = RELFACTOR(aRes, state->naiveSum);

    if (kError > globalErrors[2 * state->errorIndex]) {
      strncpy(errorLabels[2 * state->errorIndex], state->label, LABELLEN - 1);
      errorLabels[2 * state->errorIndex][LABELLEN - 1] = '\0';
      strncat(errorLabels[2 * state->errorIndex], " kahan", LABELLEN - 1);

      globalErrors[2 * state->errorIndex] = kError;
    }

    if (nError > globalErrors[2 * state->errorIndex + 1]) {
      strncpy(errorLabels[2 * state->errorIndex + 1], state->label, LABELLEN - 1);
      errorLabels[2 * state->errorIndex + 1][LABELLEN - 1] = '\0';
      strncat(errorLabels[2 * state->errorIndex + 1], " naive", LABELLEN - 1);

      globalErrors[2 * state->errorIndex + 1] = nError;
    }

    if ((configVerbose > 6) || (RELFACTOR(aRes, kRes) > DBL_EPSILON)) {
      fprintf(stderr, "%s: ", state->label);
      compareSumResults(aRes, kRes, state->naiveSum);
    }
  }

  return (aRes);
}

void compensatedSuml(struct compensatedState *state, long double x) {
  double high, low;

  high = (double)x;
  low = (double)(x - (long double)high);

  assert(state != NULL);

  adaptiveSum(&(state->aState), low);
  adaptiveSum(&(state->aState), high);
  if (configVerbose > 2) {
    state->naiveSum += high;
    kahanSum(&(state->kState), low);
    kahanSum(&(state->kState), high);
  }
}

void compensatedSum(struct compensatedState *state, double x) {
  assert(state != NULL);

  adaptiveSum(&(state->aState), x);
  if (configVerbose > 2) {
    state->naiveSum += x;
    kahanSum(&(state->kState), x);
  }
}

// state1 += scalar*state2
void compensatedAdd(struct compensatedState *state1, struct compensatedState *state2, double scalar) {
  assert(state1 != NULL);
  assert(state2 != NULL);

  adaptiveAdd(&(state1->aState), &(state2->aState), scalar);
  if (configVerbose > 2) {
    kahanAdd(&(state1->kState), &(state2->kState), scalar);
    state1->naiveSum += scalar * state2->naiveSum;
  }
}

/*Returns a value like floor, but guaranteed to be strictly less than the input.*/
/*Return value of 0.0 is a flag (in particular, any value less than 1.0) indicating no positive floor*/
/*The return value INTDOUBLE_MAX is a flag, indicating the input value is too large for integer arithmetic.*/
double positiveStrictFloor(double in) {
  double dFloor;

  // We need to verify that subtracting does something...
  if (in > INTDOUBLE_MAX) return INTDOUBLE_MAX;

  dFloor = floor(in);
  // This is baked into floor, but just in case...
  assert(dFloor <= in);

  if (dFloor < 1.0) {
    // Can't return a lower positive value. 0 is a special value for return
    return (0.0);
  } else if (dFloor < in) {
    // The proposed output is already strictly lower
    return dFloor;
  } else {
    // The proposed output is greater or equal to 1.0, but equal to the input
    // We have already verified that subtracting something from dFloor will do something
    //(it is positive and dFloor-1.0 can be represented as an integer in a double)...
    return dFloor - 1.0;
  }
}

double normalCDF(double x, double mean, double stddev) {
  double result = 0.5 * (1.0 + erf((x - mean) / (stddev * sqrt(2.0))));
  return (result);
}

uint32_t gcd(uint32_t a, uint32_t b) {
  // Make a greater a than or equal b.
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

uint64_t gcd64(uint64_t a, uint64_t b) {
  // Make a greater a than or equal b.
  if (a < b) {
    uint64_t c = a;
    a = b;
    b = c;
  }

  while (b != 0) {
    uint64_t r;

    r = a % b;

    a = b;
    b = r;
  }

  return a;
}

// Return C(n,k) > bound
// Note that C(n,k) is n * (n-1) * .. * (n-k+1) / 1 * 2 * ... * (k-1) * (k) = C(n, k-1) * (n-k+1)/k
// This is guaranteed to be an integer
// Further C(n,j) is an integer for all k >= j >= 1, so we build up using this fact.
bool combinationsGreaterThanBound(uint64_t n, uint64_t k, uint64_t bound) {
  uint64_t runningCombination = 1;
  uint64_t curn;

  // 0 can't be greater than bound.
  if (k > n) return false;

  // The binomial coefficient is symmetric
  if (n - k < k) k = n - k;

  if (configVerbose > 6) fprintf(stderr, "Checking if C(%zu, %zu) < %zu\n", n, k, bound);
  // 1 might be greater than bound.
  if (k == 0) return (1 > bound);

  curn = n;

  for (uint64_t j = 1; j <= k; j++) {
    if ((UINT64_MAX / curn) < runningCombination) {
      // The intermediate calculation would overflow
      uint64_t d = gcd64(runningCombination, j);
      uint64_t nprime;

      if (configVerbose > 6) fprintf(stderr, "binomial coefficient overflow processing... ");

      // This is r'
      runningCombination /= d;
      // j' = j / d;
      // we know that j' and r' are now relatively prime, and j' divides n * r', so j' divides n.
      nprime = curn / (j / d);
      assert(nprime > 0);
      assert(runningCombination > 0);

      // The answer is now r' * n'
      if ((UINT64_MAX / runningCombination) < nprime) {
        // Still overflows... This implies that C(n,k) >= C(n,j) > UINT64_MAX >= bound
        return true;
      } else {
        runningCombination *= nprime;
      }
    } else {
      runningCombination *= curn;
      runningCombination /= j;
    }
    // runningCombination now equals C(n,j) (which is an integer)
    if (configVerbose > 6) fprintf(stderr, "C(%zu, %zu) = %zu\n", n, j, runningCombination);
    curn--;

    if (runningCombination > bound) {
      if (configVerbose > 6) fprintf(stderr, "Over bound\n");
      return true;  // This only increases in the region we're evaluating
    } else {
      if (configVerbose > 6) fprintf(stderr, "Continuing to look.\n");
    }
  }

  if (configVerbose > 6) fprintf(stderr, "Under bound\n");
  return false;
}

// Return the number of total possible selections that are needed to meet a birthday collision probability of p=2^(-bExp)
// This is based on the approximation of the probability of a birthday collision as 1-exp(-n*(n-1)/(2*H)
// Here H is the number of selections and n is the number of rounds.
uint64_t selectionsForBirthdayCollisionBound(size_t n, uint32_t bExp) {
  uint64_t numerator;
  double dbound;

  assert(bExp < 63);

  // We can't do anything if this overflows.
  assert((UINT64_MAX / n) >= n - 1);
  numerator = n * (n - 1) / 2;
  dbound = ceil(((double)numerator / -log(1.0 - 1.0 / (double)(1UL << bExp))));

  // We can't do anything if this overflows.
  assert(dbound >= 0.0);
  assert(dbound <= (double)UINT64_MAX);

  return (uint64_t)dbound;
}

// The CDF of the binomial distribution F(k;n,p),
// that is the probability of k or fewer "successes" after n trials, where each had success probability of p.
// See https://www.itl.nist.gov/div898/handbook/eda/section3/eda366i.htm
// Note the CDF of the binomial distribution in terms of the regularized incomplete beta function is
// F(k;n,p) = I_{1-p} (n-k, k+1)
// See https://en.wikipedia.org/wiki/Binomial_distribution#Cumulative_distribution_function
double binomialCDF(size_t k, size_t n, double p) {
  assert(n >= k);
  assert(p >= DBL_MIN);
  assert(p <= 1.0);

  if (k == n) return 1.0;
  if (p < DBL_MIN) return 1.0;
  if (p > 1.0 - DBL_EPSILON) return 0.0;

  if (k < 10) {  // For small values of k, this may be helpful.
    long double lp;
    long double lq;
    long double curProb = 0.0L;
    long double lcurComb = 0.0L;  // log2(n choose 0)

    lp = log2l((long double)p);
    lq = log2l((long double)(1.0L - (long double)p));
    // Calculate F(0; n, p) = (n choose 0) * p^0 * q^(n) = q^n
    curProb = exp2l(((long double)n) * lq);

    // Starting:
    // lcurComb = log2(n choose 0) = 0.0
    // curProb = F(0; n, p)
    for (size_t i = 1; i <= k; i++) {
      long double lcurPPower;
      long double lcurPcompPower;

      // Doing the multiplication each step reduces accumulation of errors.
      // Calculate log2(p^i)
      lcurPPower = (long double)i * lp;

      // Calculate (n-i)*log2(q)
      lcurPcompPower = ((long double)(n - i)) * lq;

      // Calculate log2(C(n,  i+1))
      // Note that C(n,k) is C(n, k-1) * (n-k+1)/k
      // So we can iteratively extend at each step.
      // Note that C(n,i) = C(n, i-1) * (n-i+1)/i
      // So log2(C(n,i)) = log2(C(n, i-1)) + log2(n-i+1) - log2(i)
      lcurComb += log2l((long double)(n - i + 1)) - log2l((long double)i);

      // Calculate F(i; n, p) = F(i-1; n, p) + (n choose i) * p^i * q^(n-i)
      curProb += exp2l(lcurComb + lcurPPower + lcurPcompPower);
    }
    return (double)curProb;
  } else {
    return incbeta((double)(n - k), (double)(k + 1), 1.0 - p);
  }
}

// A general purpose comparison for sorting lists.
// Note, we _don't_ need to be fancy when testing for equality, as we'd like sorting
// to respect even insignificant differences.
int doublecompare(const void *in1, const void *in2) {
  double left;
  double right;

  assert(in1 != NULL);
  assert(in2 != NULL);

  left = *((const double *)in1);
  right = *((const double *)in2);

  assert(!isnan(left) && !isnan(right));

  if (left < right) {
    return -1;
  } else if (left > right) {
    return 1;
  } else {
    return 0;
  }
}
