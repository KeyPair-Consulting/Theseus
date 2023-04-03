/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef FANCYMATH_H
#define FANCYMATH_H

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <limits.h>
#include "globals.h"

#ifndef DBL_INFINITY
#define DBL_INFINITY __builtin_inf()
#endif

#define NORMALOUTLIER 7.0477002566644087
#define ZALPHA 2.5758293035489008
#define ZALPHA_L 2.575829303548900384158L
#define INTDOUBLE_MAX 9007199254740992.0
#define ROUGHEPISILON 1.0E-6
// This is the smallest practical value (one can't do better with the double type)
#define RELEPSILON DBL_EPSILON
// // This is clearly overkill, but it's difficult to do better without a view into the monotonic function
#define ABSEPSILON DBL_MIN
//Similarly, but for long doubles

#define LRELEPSILON LDBL_EPSILON
#define LABSEPSILON LDBL_MIN

#define ULPEPSILON 4

//#define KAHANEXTENDEDPRECISION

#ifdef KAHANEXTENDEDPRECISION
#define KAHANACCUMULATOR long double
#else
#define KAHANACCUMULATOR double
#endif

#define RELFACTOR(x, y) (relEpsilonEqual((x), (y), ABSEPSILON, RELEPSILON, ULPEPSILON) ? 0.0 : ((fabs(x) > fabs(y)) ? fabs((x) - (y)) / fabs(x) : (fabs((y) - (x)) / fabs(y))))
#define RELFACTORL(x, y) ((fabsl(x) > fabsl(y)) ? fabsl((x) - (y)) / fabsl(x) : ((fabsl(y) > 0.0L) ? (fabsl((x) - (y)) / fabsl(y)) : 0.0L))

#define INOPENINTERVAL(x, a, b) (((a) > (b)) ? (((x) > (b)) && ((x) < (a))) : (((x) > (a)) && ((x) < (b))))
#define INCLOSEDINTERVAL(x, a, b) (((a) > (b)) ? (((x) >= (b)) && ((x) <= (a))) : (((x) >= (a)) && ((x) <= (b))))
#define DEBUGCLOSE(A, B)                                                                                                   \
  do {                                                                                                                     \
    if (!relEpsilonEqual((A), (B), ROUGHEPISILON, ROUGHEPISILON, 4)) fprintf(stderr, "DEBUG: %.17g != %.17g\n", (A), (B)); \
    assert(relEpsilonEqual((A), (B), ROUGHEPISILON, ROUGHEPISILON, 4));                                                    \
  } while (0)

//Make uint128_t a supported type (standard as of C23)
#ifdef __SIZEOF_INT128__
typedef unsigned __int128 uint128_t;
typedef unsigned __int128 uint_least128_t;
# define UINT128_MAX         ((uint128_t)-1)
# define UINT128_WIDTH       128
# define UINT_LEAST128_WIDTH 128
# define UINT_LEAST128_MAX   UINT128_MAX
# define UINT128_C(N)        ((uint_least128_t)+N ## WBU)
#endif
static inline double square(double x) {
  return x * x;
}

static inline size_t nextBinaryPower(size_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  x++;
  return (x);
}

/* The next three routines provide square-and-multiply exponentiation (for integer exponents),
 * as this approach is both more accurate and dramatically faster than the more general pow()
 * function.
 */
static inline long double integerPowl(long double x, uint64_t exponent) {
  long double y;
  if (configVerbose > 6) fprintf(stderr, "integerPowl: %.17Lg ^ %" PRIu64 " = ", x, exponent);

  if (exponent == 0) return 1.0L;

  y = 1.0L;
  while (exponent != 1) {
    if ((exponent & 0x01) != 0) y *= x;
    x *= x;
    exponent >>= 1;
  }

  // We now are left with the trailing bit, which needs to be multiplied in
  y *= x;

  if (configVerbose > 6) fprintf(stderr, "%.17Lg\n", y);
  return y;
}

static inline double integerPow(double x, uint64_t exponent) {
  double y;

  if (configVerbose > 6) fprintf(stderr, "integerPow: %.17g ^ %" PRIu64 " = ", x, exponent);
  if (exponent == 0) return 1.0;

  y = 1.0;
  while (exponent != 1) {
    if ((exponent & 0x01) != 0) y *= x;
    x *= x;
    exponent >>= 1;
  }

  // We now are left with the trailing bit, which needs to be multiplied in
  y *= x;

  if (configVerbose > 6) fprintf(stderr, "%.17g\n", y);
  return y;
}

#define LABELLEN 64
struct kahanState {
  KAHANACCUMULATOR sum;
  KAHANACCUMULATOR compensation;
};

struct adaptiveState {
  double *partials;
  size_t numOfPartials;
  size_t partialListCapacity;
};

struct compensatedState {
  struct adaptiveState aState;
  struct kahanState kState;
  double naiveSum;
  size_t errorIndex;
  char label[LABELLEN];
};

#define PARTIALBLOCK 1024

#define DOUBLEASSERTEQUALITY(a, b) verboseDoubleAssertEquality((a), (b), __FILE__, __func__, __LINE__)

bool relEpsilonEqual(double A, double B, double maxAbsFactor, double maxRelFactor, uint64_t maxULP);
bool relEpsilonEquall(long double A, long double B, long double maxAbsFactor, long double maxRelFactor, uint64_t maxULP);

void initCompensatedSum(struct compensatedState *state, const char *label, size_t errorIndex);
void delCompensatedSum(struct compensatedState *state);
void compensatedSum(struct compensatedState *state, double x);
void compensatedSuml(struct compensatedState *state, long double x);
void compensatedAdd(struct compensatedState *state1, struct compensatedState *state2, double scalar);
double compensatedSumResult(struct compensatedState *state);
long double compensatedSumResultl(struct compensatedState *state);
double positiveStrictFloor(double in);
double normalCDF(double x, double mean, double stddev);
uint32_t gcd(uint32_t a, uint32_t b);
uint64_t gcd64(uint64_t a, uint64_t b);
bool combinationsGreaterThanBound(uint64_t n, uint64_t k, uint64_t bound);
uint64_t selectionsForBirthdayCollisionBound(size_t n, uint32_t bExp);
double binomialCDF(size_t k, size_t n, double p);
int doublecompare(const void *in1, const void *in2);
double monotonicBinarySearch(double (*fval)(double, const size_t *), double ldomain, double hdomain, double target, const size_t *params, bool decreasing);
void safeAdduint64(uint64_t a, uint64_t b, uint64_t *res);
void safeAdduint128(uint128_t a, uint128_t b, uint128_t *res);
char *uint128ToString(uint128_t in, char *buffer);
void verboseDoubleAssertEquality(double a, double b, const char *file, const char *function, int line);
#endif
