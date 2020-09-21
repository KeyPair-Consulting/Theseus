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
 * Author(s)
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University).
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University and The University of Tokyo).
 *  Mutsuo Saito, Makoto Matsumoto (Hiroshima University).
 *  Joshua E. Hill, UL VS LLC.
 *  Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */

#ifndef SFMTST_H
#define SFMTST_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include <inttypes.h>

#define SFMT_MEXP 19937
/*-----------------
  BASIC DEFINITIONS
  -----------------*/
/** Mersenne Exponent. The period of the sequence
 *  is a multiple of 2^MEXP-1.
 * #define SFMT_MEXP 19937 */
/** SFMT generator has an internal state array of 128-bit integers,
 * and N is its size. */
#define SFMT_N (SFMT_MEXP / 128 + 1)
/** N32 is the size of internal state array when regarded as an array
 * of 32-bit integers.*/
#define SFMT_N32 (SFMT_N * 4)
/** N64 is the size of internal state array when regarded as an array
 * of 64-bit integers.*/
#define SFMT_N64 (SFMT_N * 2)


#define SFMT_POS1 122
#define SFMT_SL1 18
#define SFMT_SL2 1
#define SFMT_SR1 11
#define SFMT_SR2 1
#define SFMT_MSK1 0xdfffffefU
#define SFMT_MSK2 0xddfecb7fU
#define SFMT_MSK3 0xbffaffffU
#define SFMT_MSK4 0xbffffff6U
#define SFMT_PARITY1 0x00000001U
#define SFMT_PARITY2 0x00000000U
#define SFMT_PARITY3 0x00000000U
#define SFMT_PARITY4 0x13c9e684U

#define SFMT_IDSTR "SFMT-19937:122-18-1-11-1:dfffffef-ddfecb7f-bffaffff-bffffff6"
#include "precision.h"

#ifdef SSE2
#define HAVE_SSE2 1
#endif

/*------------------------------------------
  128-bit SIMD like data type for standard C
  ------------------------------------------*/
#if defined(HAVE_SSE2)
#include <emmintrin.h>

/** 128-bit data structure */
union W128_T {
  uint32_t u[4];
  uint64_t u64[2];
  __m128i si;
};
#else
/** 128-bit data structure */
union W128_T {
  uint32_t u[4];
  uint64_t u64[2];
};
#endif

/** 128-bit data type */
typedef union W128_T w128_t;

/**
 * SFMT internal state
 */
struct SFMT_T {
  /** the 128-bit internal state array */
  w128_t state[SFMT_N];
  /** index counter to the 32-bit internal state array */
  int idx;
};

typedef struct SFMT_T sfmt_t;

void sfmt_fill_array32(sfmt_t* sfmt, uint32_t* array, int size);
void sfmt_fill_array64(sfmt_t* sfmt, uint64_t* array, int size);
void sfmt_init_gen_rand(sfmt_t* sfmt, uint32_t seed);
void sfmt_init_by_array(sfmt_t* sfmt, uint32_t* init_key, int key_length);
const char* sfmt_get_idstring(sfmt_t* sfmt);
int sfmt_get_min_array_size32(sfmt_t* sfmt);
int sfmt_get_min_array_size64(sfmt_t* sfmt);
void sfmt_gen_rand_all(sfmt_t* sfmt);

/**
 * This function generates and returns 32-bit pseudorandom number.
 * init_gen_rand or init_by_array must be called before this function.
 * @param sfmt SFMT internal state
 * @return 32-bit pseudorandom number
 */
inline static uint32_t sfmt_genrand_uint32(sfmt_t* sfmt) {
  uint32_t r;
  uint32_t* psfmt32 = &sfmt->state[0].u[0];

  if (sfmt->idx >= SFMT_N32) {
    sfmt_gen_rand_all(sfmt);
    sfmt->idx = 0;
  }
  r = psfmt32[sfmt->idx++];
  return r;
}
/**
 * This function generates and returns 64-bit pseudorandom number.
 * init_gen_rand or init_by_array must be called before this function.
 * The function gen_rand64 should not be called after gen_rand32,
 * unless an initialization is again executed.
 * @param sfmt SFMT internal state
 * @return 64-bit pseudorandom number
 */
inline static uint64_t sfmt_genrand_uint64(sfmt_t* sfmt) {
  uint64_t r;
  uint64_t* psfmt64 = &sfmt->state[0].u64[0];
  assert(sfmt->idx % 2 == 0);

  if (sfmt->idx >= SFMT_N32) {
    sfmt_gen_rand_all(sfmt);
    sfmt->idx = 0;
  }
  r = psfmt64[sfmt->idx / 2];
  sfmt->idx += 2;
  return r;
}

/* =================================================
   The following real versions are due to Isaku Wada
   ================================================= */
/**
 * converts an unsigned 32-bit number to a double on [0,1]-real-interval.
 * @param v 32-bit unsigned integer
 * @return double on [0,1]-real-interval
 */
inline static double sfmt_to_real1(uint32_t v) {
  return v * (1.0 / 4294967295.0);
  /* divided by 2^32-1 */
}

/**
 * generates a random number on [0,1]-real-interval
 * @param sfmt SFMT internal state
 * @return double on [0,1]-real-interval
 */
inline static double sfmt_genrand_real1(sfmt_t* sfmt) {
  return sfmt_to_real1(sfmt_genrand_uint32(sfmt));
}

/**
 * converts an unsigned 32-bit integer to a double on [0,1)-real-interval.
 * @param v 32-bit unsigned integer
 * @return double on [0,1)-real-interval
 */
inline static double sfmt_to_real2(uint32_t v) {
  return v * (1.0 / 4294967296.0);
  /* divided by 2^32 */
}

/**
 * generates a random number on [0,1)-real-interval
 * @param sfmt SFMT internal state
 * @return double on [0,1)-real-interval
 */
inline static double sfmt_genrand_real2(sfmt_t* sfmt) {
  return sfmt_to_real2(sfmt_genrand_uint32(sfmt));
}

/**
 * converts an unsigned 32-bit integer to a double on (0,1)-real-interval.
 * @param v 32-bit unsigned integer
 * @return double on (0,1)-real-interval
 */
inline static double sfmt_to_real3(uint32_t v) {
  return (((double)v) + 0.5) * (1.0 / 4294967296.0);
  /* divided by 2^32 */
}

/**
 * generates a random number on (0,1)-real-interval
 * @param sfmt SFMT internal state
 * @return double on (0,1)-real-interval
 */
inline static double sfmt_genrand_real3(sfmt_t* sfmt) {
  return sfmt_to_real3(sfmt_genrand_uint32(sfmt));
}

/* =================================================
   The following function are added by Saito.
   ================================================= */
#endif
