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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#if defined(__x86_64) || defined(__x86_64__)
#include <x86intrin.h>
#endif

#include "binutil.h"
#include "entlib.h"
#include "globals.h"
#include "precision.h"

uint32_t extractbits(const uint32_t input, const uint32_t bitMask) {
#ifndef BMI2
  /*Taken from Hacker's Delight, 2nd edition, pp 153*/
  uint32_t mk, mp, mv, t, m;
  uint32_t i;
  unsigned x;

  m = bitMask;
  x = input;

  x = x & m;  // Clear irrelevant bits.
  mk = ~m << 1U;  // We will count 0's to right.

  for (i = 0; i < 5; i++) {
    mp = mk ^ (mk << 1U);  // Parallel suffix.
    mp = mp ^ (mp << 2U);
    mp = mp ^ (mp << 4U);
    mp = mp ^ (mp << 8U);
    mp = mp ^ (mp << 16U);
    mv = mp & m;  // Bits to move.
    m = (m ^ mv) | (mv >> (1U << i));  // Compress m.
    t = x & mv;
    x = (x ^ t) | (t >> (1U << i));  // Compress x.
    mk = mk & ~mp;
  }
  // assert(_pext_u32(input, bitMask)==x);
  return x;
#else
  return (_pext_u32(input, bitMask));
#endif
}

void extractbitsArray(const uint32_t *input, statData_t *output, const size_t datalen, const uint32_t bitMask) {
  size_t i;
#ifndef BMI2
  /*Taken from Hacker's Delight, 2nd edition, pp 153-154*/
  uint32_t masks[5];
  uint32_t mv0, mv1, mv2, mv3, mv4;
  uint32_t q, m, zm, x;

  assert(__builtin_popcount(bitMask) <= STATDATA_BITS);

  /*Prep the mv0 to mv4 constants for the eventual reuse throughout the array*/
  m = ~bitMask;
  zm = bitMask;
  for (i = 0; i < 5; i++) {
    q = m;
    m ^= m << 1;
    m ^= m << 2;
    m ^= m << 4;
    m ^= m << 8;
    m ^= m << 16;
    masks[i] = (m << 1) & zm;
    m = q & ~m;
    q = zm & masks[i];
    zm = zm ^ q ^ (q >> (1 << i));
  }

  mv0 = masks[0];
  mv1 = masks[1];
  mv2 = masks[2];
  mv3 = masks[3];
  mv4 = masks[4];

  for (i = 0; i < datalen; i++) {
    x = input[i] & bitMask;  // input
    q = x & mv0;
    x = x ^ q ^ (q >> 1);
    q = x & mv1;
    x = x ^ q ^ (q >> 2);
    q = x & mv2;
    x = x ^ q ^ (q >> 4);
    q = x & mv3;
    x = x ^ q ^ (q >> 8);
    q = x & mv4;
    output[i] = (statData_t)(x ^ q ^ (q >> 16));  // output
    // assert(output[i]==_pext_u32(input[i], bitMask));
  }

#else
  assert(__builtin_popcount(bitMask) <= STATDATA_BITS);
  for (i = 0; i < datalen; i++) {
    output[i] = (statData_t)_pext_u32(input[i], bitMask);
  }
#endif
}

uint32_t expandBits(const uint32_t input, const uint32_t bitMask) {
#ifndef BMI2
  uint32_t m0, mk, mp, mv, t, x, m;
  uint32_t array[5];
  int i;

  m = bitMask;
  x = input;

  m0 = m;  // Save original mask.
  mk = ~m << 1U;  // We will count 0's to right.

  for (i = 0; i < 5; i++) {
    mp = mk ^ (mk << 1U);  // Parallel suffix.
    mp = mp ^ (mp << 2U);
    mp = mp ^ (mp << 4U);
    mp = mp ^ (mp << 8U);
    mp = mp ^ (mp << 16U);
    mv = mp & m;  // Bits to move.
    array[i] = mv;
    m = (m ^ mv) | (mv >> (1U << i));  // Compress m.
    mk = mk & ~mp;
  }

  for (i = 4; i >= 0; i--) {
    mv = array[i];
    t = x << (1U << i);
    x = (x & ~mv) | (t & mv);
    //    x = ((x ^ t) & mv) ^ x;           // Alternative for above line.
  }
  // assert((x & m0) == _pdep_u32(input, bitMask));
  return x & m0;  // Clear out extraneous bits.
#else
  return (_pdep_u32(input, bitMask));
#endif
}

uint32_t getActiveBits(const uint32_t *data, size_t datalen) {
  uint32_t andmask, ormask;
  uint32_t andRelated[32];
  uint32_t orRelated[32];
  uint32_t bitmask;
  uint32_t curIndep;
  uint32_t discardBits;
  uint32_t activeBits;

  andmask = UINT32_MAX;
  ormask = 0;
  for (size_t i = 0; i < 32; i++) {
    andRelated[i] = UINT32_MAX;
    orRelated[i] = 0;
  }

  for (size_t i = 0; i < datalen; i++) {
    andmask &= data[i];
    ormask |= data[i];
    bitmask = 1;
    for (size_t j = 0; j < 32; j++) {
      if ((bitmask & data[i]) == 0) {
        andRelated[j] &= ~(data[i]);
        orRelated[j] |= ~(data[i]);
      } else {
        andRelated[j] &= data[i];
        orRelated[j] |= data[i];
      }

      bitmask <<= 1;
    }
  }

  // First, look for any fixed bits.
  activeBits = ~andmask & ormask;

  if (configVerbose > 0) fprintf(stderr, "Non-fixed bits: 0x%08X.\n", activeBits);

  // Now, look for equivalent bits (bits that are always equal to other places, or are alway the bitwise compliment of other places)
  bitmask = 0x80000000;
  for (int j = 31; j >= 0; j--) {
    if (activeBits & bitmask) {
      curIndep = (~andRelated[j] & orRelated[j]) | bitmask;
      discardBits = (~curIndep) & activeBits;
      if (discardBits != 0) {
        if (configVerbose > 0) fprintf(stderr, "Discarding bits equivalent to bit %d: 0x%08X.\n", j, discardBits);
        activeBits &= curIndep;
      }
    }
    bitmask >>= 1;
  }

  if (configVerbose > 0) fprintf(stderr, "Bits to analyze: 0x%08X.\n", activeBits);
  return activeBits;
}

statData_t getActiveBitsSD(const statData_t *data, size_t datalen) {
  statData_t andmask, ormask;
  statData_t andRelated[STATDATA_BITS];
  statData_t orRelated[STATDATA_BITS];
  statData_t bitmask;
  statData_t curIndep;
  statData_t discardBits;
  statData_t activeBits;

  andmask = STATDATA_MAX;
  ormask = 0;
  for (size_t i = 0; i < STATDATA_BITS; i++) {
    andRelated[i] = STATDATA_MAX;
    orRelated[i] = 0;
  }

  for (size_t i = 0; i < datalen; i++) {
    andmask &= data[i];
    ormask |= data[i];
    bitmask = 1;
    for (size_t j = 0; j < STATDATA_BITS; j++) {
      if ((bitmask & data[i]) == 0) {
        andRelated[j] = (statData_t)(andRelated[j] & ~(data[i]));
        orRelated[j] = (statData_t)(orRelated[j] | ~(data[i]));
      } else {
        andRelated[j] = (statData_t)(andRelated[j] & data[i]);
        orRelated[j] = (statData_t)(orRelated[j] | data[i]);
      }

      bitmask = (statData_t)(bitmask << 1);
    }
  }

  activeBits = (statData_t)(~andmask & ormask);

  if (configVerbose > 0) fprintf(stderr, "Non-fixed bits: 0x%08X.\n", activeBits);

  bitmask = (statData_t)(1U << (STATDATA_BITS - 1));
  for (int j = STATDATA_BITS - 1; j >= 0; j--) {
    if (activeBits & bitmask) {
      curIndep = (statData_t)((~andRelated[j] & orRelated[j]) | bitmask);
      discardBits = (statData_t)((~curIndep) & activeBits);
      if (discardBits != 0) {
        if (configVerbose > 0) fprintf(stderr, "Discarding bits equivalent to bit %d: 0x%08X.\n", j, discardBits);
        activeBits &= curIndep;
      }
    }
    bitmask = (statData_t)(bitmask >> 1);
  }

  if (configVerbose > 0) fprintf(stderr, "Bits to analyze: 0x%08X.\n", activeBits);
  return activeBits;
}

statData_t lowBit(statData_t in) {
  return (statData_t)(in & (-in));
}

// See Hacker's Delight, Second Edition, page 12
uint32_t u32lowBit(uint32_t in) {
  return in & (-in);
}

uint32_t u32highBit(uint32_t in) {
  in |= in >> 1;
  in |= in >> 2;
  in |= in >> 4;
  in |= in >> 8;
  in |= in >> 16;

  return (in - (in >> 1));
}

// See Hacker's Delight, Second Edition, page 61
statData_t highBit(statData_t in) {
  in |= in >> 1;
  in |= in >> 2;
  in |= in >> 4;
#if STATDATA_BITS == 32
  in |= in >> 8;
  in |= in >> 16;
#endif

  return (statData_t)(in - (statData_t)(in >> 1));
}

uint32_t reverse32(uint32_t input) {
  return (((0xFF000000U & input) >> (3 * 8)) | ((0x00FF0000U & input) >> (1 * 8)) | ((0x0000FF00U & input) << (1 * 8)) | ((0x000000FFU & input) << (3 * 8)));
}

uint64_t reverse64(uint64_t input) {
  uint64_t out;

  out = (((0xFF00000000000000UL & input) >> (7 * 8)) | ((0x00FF000000000000UL & input) >> (5 * 8)) | ((0x0000FF0000000000UL & input) >> (3 * 8)) | ((0x000000FF00000000UL & input) >> (1 * 8)) | ((0x00000000FF000000UL & input) << (1 * 8)) |
         ((0x0000000000FF0000UL & input) << (3 * 8)) | ((0x000000000000FF00UL & input) << (5 * 8)) | ((0x00000000000000FFUL & input) << (7 * 8)));
  return (out);
}

void reverse128(uint64_t *input) {
  uint64_t temp;

  assert(input != NULL);

  temp = reverse64(input[0]);
  input[0] = reverse64(input[1]);
  input[1] = temp;
}
