/**
 * Copyright (C) 2006, 2007 Mutsuo Saito, Makoto Matsumoto and Hiroshima
 * University.
 * Copyright (C) 2012 Mutsuo Saito, Makoto Matsumoto, Hiroshima
 * University and The University of Tokyo.
 * Copyright (C) 2013 Mutsuo Saito, Makoto Matsumoto and Hiroshima
 * University.
 * All rights reserved.
 */
/* This file is part of the Theseus distribution, and is licensed under
 * the 3-clause BSD license. For details, see the LICENSE file.
 *
 * This is a specialization of the SFMT 1.5.1 distribution from here:
 * http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/SFMT/
 * This specialization supports only little endian architectures in modern
 * Linux-like environments, does not support NEON or AltiVEC improvements
 * present in the mainline package, and includes only the 19937 parameter set.
 *
 * Author(s)
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University).
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University and The University of Tokyo).
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University).
 *  Joshua E. Hill, UL VS LLC.
 *  Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>

 */

#include "SFMT.h"
#include <assert.h>
#include <string.h>

/*----------------
  STATIC FUNCTIONS
  ----------------*/
inline static uint32_t func1(uint32_t x);
inline static uint32_t func2(uint32_t x);
static void period_certification(sfmt_t *sfmt);

#if defined(HAVE_SSE2)
/**
 * parameters used by sse2.
 */
static const w128_t sse2_param_mask = {{SFMT_MSK1, SFMT_MSK2, SFMT_MSK3, SFMT_MSK4}};

inline static void mm_recursion(__m128i *r, __m128i a, __m128i b, __m128i c, __m128i d);

/**
 * This function represents the recursion formula.
 * @param r an output
 * @param a a 128-bit part of the interal state array
 * @param b a 128-bit part of the interal state array
 * @param c a 128-bit part of the interal state array
 * @param d a 128-bit part of the interal state array
 */
inline static void mm_recursion(__m128i *r, __m128i a, __m128i b, __m128i c, __m128i d) {
  __m128i v, x, y, z;

  y = _mm_srli_epi32(b, SFMT_SR1);
  z = _mm_srli_si128(c, SFMT_SR2);
  v = _mm_slli_epi32(d, SFMT_SL1);
  z = _mm_xor_si128(z, a);
  z = _mm_xor_si128(z, v);
  x = _mm_slli_si128(a, SFMT_SL2);
  y = _mm_and_si128(y, sse2_param_mask.si);
  z = _mm_xor_si128(z, x);
  z = _mm_xor_si128(z, y);
  *r = z;
}

/**
 * This function fills the internal state array with pseudorandom
 * integers.
 * @param sfmt SFMT internal state
 */
void sfmt_gen_rand_all(sfmt_t *sfmt) {
  int i;
  __m128i r1, r2;
  w128_t *pstate = sfmt->state;

  r1 = pstate[SFMT_N - 2].si;
  r2 = pstate[SFMT_N - 1].si;
  for (i = 0; i < SFMT_N - SFMT_POS1; i++) {
    mm_recursion(&pstate[i].si, pstate[i].si, pstate[i + SFMT_POS1].si, r1, r2);
    r1 = r2;
    r2 = pstate[i].si;
  }
  for (; i < SFMT_N; i++) {
    mm_recursion(&pstate[i].si, pstate[i].si, pstate[i + SFMT_POS1 - SFMT_N].si, r1, r2);
    r1 = r2;
    r2 = pstate[i].si;
  }
}

#else

inline static void rshift128(w128_t *out, w128_t const *in, int shift);
inline static void lshift128(w128_t *out, w128_t const *in, int shift);

inline static void do_recursion(w128_t *r, w128_t *a, w128_t *b, w128_t *c, w128_t *d);

inline static void rshift128(w128_t *out, w128_t const *in, int shift) {
  uint64_t th, tl, oh, ol;

  th = ((uint64_t)in->u[3] << 32) | ((uint64_t)in->u[2]);
  tl = ((uint64_t)in->u[1] << 32) | ((uint64_t)in->u[0]);

  oh = th >> (shift * 8);
  ol = tl >> (shift * 8);
  ol |= th << (64 - shift * 8);
  out->u[1] = (uint32_t)(ol >> 32);
  out->u[0] = (uint32_t)ol;
  out->u[3] = (uint32_t)(oh >> 32);
  out->u[2] = (uint32_t)oh;
}

inline static void lshift128(w128_t *out, w128_t const *in, int shift) {
  uint64_t th, tl, oh, ol;

  th = ((uint64_t)in->u[3] << 32) | ((uint64_t)in->u[2]);
  tl = ((uint64_t)in->u[1] << 32) | ((uint64_t)in->u[0]);

  oh = th << (shift * 8);
  ol = tl << (shift * 8);
  oh |= tl >> (64 - shift * 8);
  out->u[1] = (uint32_t)(ol >> 32);
  out->u[0] = (uint32_t)ol;
  out->u[3] = (uint32_t)(oh >> 32);
  out->u[2] = (uint32_t)oh;
}

inline static void do_recursion(w128_t *r, w128_t *a, w128_t *b, w128_t *c, w128_t *d) {
  w128_t x;
  w128_t y;

  lshift128(&x, a, SFMT_SL2);
  rshift128(&y, c, SFMT_SR2);
  r->u[0] = a->u[0] ^ x.u[0] ^ ((b->u[0] >> SFMT_SR1) & SFMT_MSK1) ^ y.u[0] ^ (d->u[0] << SFMT_SL1);
  r->u[1] = a->u[1] ^ x.u[1] ^ ((b->u[1] >> SFMT_SR1) & SFMT_MSK2) ^ y.u[1] ^ (d->u[1] << SFMT_SL1);
  r->u[2] = a->u[2] ^ x.u[2] ^ ((b->u[2] >> SFMT_SR1) & SFMT_MSK3) ^ y.u[2] ^ (d->u[2] << SFMT_SL1);
  r->u[3] = a->u[3] ^ x.u[3] ^ ((b->u[3] >> SFMT_SR1) & SFMT_MSK4) ^ y.u[3] ^ (d->u[3] << SFMT_SL1);
}

/**
 * This function fills the internal state array with pseudorandom
 * integers.
 * @param sfmt SFMT internal state
 */
void sfmt_gen_rand_all(sfmt_t *sfmt) {
  int i;
  w128_t *r1, *r2;

  r1 = &sfmt->state[SFMT_N - 2];
  r2 = &sfmt->state[SFMT_N - 1];
  for (i = 0; i < SFMT_N - SFMT_POS1; i++) {
    do_recursion(&sfmt->state[i], &sfmt->state[i], &sfmt->state[i + SFMT_POS1], r1, r2);
    r1 = r2;
    r2 = &sfmt->state[i];
  }
  for (; i < SFMT_N; i++) {
    do_recursion(&sfmt->state[i], &sfmt->state[i], &sfmt->state[i + SFMT_POS1 - SFMT_N], r1, r2);
    r1 = r2;
    r2 = &sfmt->state[i];
  }
}

#endif

/**
 * This function represents a function used in the initialization
 * by init_by_array
 * @param x 32-bit integer
 * @return 32-bit integer
 */
static uint32_t func1(uint32_t x) {
  return (x ^ (x >> 27)) * (uint32_t)1664525UL;
}

/**
 * This function represents a function used in the initialization
 * by init_by_array
 * @param x 32-bit integer
 * @return 32-bit integer
 */
static uint32_t func2(uint32_t x) {
  return (x ^ (x >> 27)) * (uint32_t)1566083941UL;
}

/**
 * This function certificate the period of 2^{MEXP}
 * @param sfmt SFMT internal state
 */
static void period_certification(sfmt_t *sfmt) {
  uint32_t inner = 0;
  int i, j;
  uint32_t work;
  uint32_t *psfmt32 = &sfmt->state[0].u[0];
  const uint32_t parity[4] = {SFMT_PARITY1, SFMT_PARITY2, SFMT_PARITY3, SFMT_PARITY4};

  for (i = 0; i < 4; i++) {
    inner ^= psfmt32[i] & parity[i];
  }
  for (i = 16; i > 0; i >>= 1) {
    inner ^= inner >> i;
  }
  inner &= 1;
  /* check OK */
  if (inner == 1) {
    return;
  }
  /* check NG, and modification */
  for (i = 0; i < 4; i++) {
    work = 1;
    for (j = 0; j < 32; j++) {
      if ((work & parity[i]) != 0) {
        psfmt32[i] ^= work;
        return;
      }
      work = work << 1;
    }
  }
}

/*----------------
  PUBLIC FUNCTIONS
  ----------------*/
#define UNUSED_VARIABLE(x) (void)(x)
/**
 * This function returns the identification string.
 * The string shows the word size, the Mersenne exponent,
 * and all parameters of this generator.
 * @param sfmt SFMT internal state
 */
const char *sfmt_get_idstring(sfmt_t *sfmt) {
  UNUSED_VARIABLE(sfmt);
  return SFMT_IDSTR;
}

/**
 * This function returns the minimum size of array used for \b
 * fill_array32() function.
 * @param sfmt SFMT internal state
 * @return minimum size of array used for fill_array32() function.
 */
int sfmt_get_min_array_size32(sfmt_t *sfmt) {
  UNUSED_VARIABLE(sfmt);
  return SFMT_N32;
}

/**
 * This function returns the minimum size of array used for \b
 * fill_array64() function.
 * @param sfmt SFMT internal state
 * @return minimum size of array used for fill_array64() function.
 */
int sfmt_get_min_array_size64(sfmt_t *sfmt) {
  UNUSED_VARIABLE(sfmt);
  return SFMT_N64;
}

/**
 * This function initializes the internal state array,
 * with an array of 32-bit integers used as the seeds
 * @param sfmt SFMT internal state
 * @param init_key the array of 32-bit integers, used as a seed.
 * @param key_length the length of init_key.
 */
void sfmt_init_by_array(sfmt_t *sfmt, uint32_t *init_key, int key_length) {
  int i, j, count;
  uint32_t r;
  int lag;
  int mid;
  int size = SFMT_N * 4;
  uint32_t *psfmt32 = &sfmt->state[0].u[0];

  if (size >= 623) {
    lag = 11;
  } else if (size >= 68) {
    lag = 7;
  } else if (size >= 39) {
    lag = 5;
  } else {
    lag = 3;
  }
  mid = (size - lag) / 2;

  memset(sfmt, 0x8b, sizeof(sfmt_t));
  if (key_length + 1 > SFMT_N32) {
    count = key_length + 1;
  } else {
    count = SFMT_N32;
  }
  r = func1(psfmt32[0] ^ psfmt32[mid] ^ psfmt32[SFMT_N32 - 1]);
  psfmt32[mid] += r;
  r += (unsigned int)key_length;
  psfmt32[mid + lag] += r;
  psfmt32[0] = r;

  count--;
  for (i = 1, j = 0; (j < count) && (j < key_length); j++) {
    r = func1(psfmt32[i] ^ psfmt32[(i + mid) % SFMT_N32] ^ psfmt32[(i + SFMT_N32 - 1) % SFMT_N32]);
    psfmt32[(i + mid) % SFMT_N32] += r;
    r += init_key[j] + (unsigned int)i;
    psfmt32[(i + mid + lag) % SFMT_N32] += r;
    psfmt32[i] = r;
    i = (i + 1) % SFMT_N32;
  }
  for (; j < count; j++) {
    r = func1(psfmt32[i] ^ psfmt32[(i + mid) % SFMT_N32] ^ psfmt32[(i + SFMT_N32 - 1) % SFMT_N32]);
    psfmt32[(i + mid) % SFMT_N32] += r;
    r += (unsigned int)i;
    psfmt32[(i + mid + lag) % SFMT_N32] += r;
    psfmt32[i] = r;
    i = (i + 1) % SFMT_N32;
  }
  for (j = 0; j < SFMT_N32; j++) {
    r = func2(psfmt32[i] + psfmt32[(i + mid) % SFMT_N32] + psfmt32[(i + SFMT_N32 - 1) % SFMT_N32]);
    psfmt32[(i + mid) % SFMT_N32] ^= r;
    r -= (unsigned int)i;
    psfmt32[(i + mid + lag) % SFMT_N32] ^= r;
    psfmt32[i] = r;
    i = (i + 1) % SFMT_N32;
  }

  sfmt->idx = SFMT_N32;
  period_certification(sfmt);
}
