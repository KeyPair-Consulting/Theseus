/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <cpuid.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef bit_SSE4_2
#define bit_SSE4_2 (1 << 20)
#endif

#ifndef bit_BMI2
#define bit_BMI2 (1 << 20)
#endif

int main(void) {
  uint32_t eax = 0;
  uint32_t ebx = 0;
  uint32_t ecx = 0;
  uint32_t edx = 0;
  uint32_t ids = 0;
  printf("#ifndef PRECISION_H\n");
  printf("#define PRECISION_H\n");
#if __GNUC__ > 4
  printf("#define PRECISION(value) _Generic(value, \\\n");
  printf("  unsigned char : %d, \\\n", __builtin_popcountll((uintmax_t)UCHAR_MAX));
  printf("  unsigned short : %d, \\\n", __builtin_popcountll((uintmax_t)USHRT_MAX));
  printf("  unsigned int : %d, \\\n", __builtin_popcountll((uintmax_t)UINT_MAX));
  printf("  unsigned long : %d, \\\n", __builtin_popcountll((uintmax_t)ULONG_MAX));
  printf("  unsigned long long : %d, \\\n", __builtin_popcountll((uintmax_t)ULLONG_MAX));
  printf("  signed char : %d, \\\n", __builtin_popcountll((uintmax_t)SCHAR_MAX));
  printf("  signed short : %d, \\\n", __builtin_popcountll((uintmax_t)SHRT_MAX));
  printf("  signed int : %d, \\\n", __builtin_popcountll((uintmax_t)INT_MAX));
  printf("  signed long : %d, \\\n", __builtin_popcountll((uintmax_t)LONG_MAX));
  printf("  signed long long : %d)\n", __builtin_popcountll((uintmax_t)LLONG_MAX));
#else
  printf("#define PRECISION(umax_value) __builtin_popcountll(umax_value)\n");
#endif

  __get_cpuid(0, &eax, &ebx, &ecx, &edx);
  ids = eax;

  if (ids >= 1) {
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    if (ecx & bit_SSE4_2) {
      printf("#define SSE42\n");
    }
    if (ecx & bit_SSE4_1) {
      printf("#define SSE41\n");
    }
    if (ecx & bit_SSE3) {
      printf("#define SSE3\n");
    }
    if (ecx & bit_RDRND) {
      printf("#define RDRND\n");
    }
    if (ecx & bit_POPCNT) {
      printf("#define POPCNT\n");
    }
    if (edx & bit_SSE) {
      printf("#define SSE\n");
    }
    if (edx & bit_SSE2) {
      printf("#define SSE2\n");
    }
  }

  if (ids >= 7) {
    __get_cpuid(7, &eax, &ebx, &ecx, &edx);
    if (ebx & bit_BMI2) {
      printf("#define BMI2\n");
    }
  }

  printf("\n");
  printf("#endif\n");

  return 0;
}
