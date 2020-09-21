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
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "globals-inst.h"
#include "randlib.h"

// Make sure to lock this to a single core when testing.
// make the entire memory block 2^24 bytes
#define BLOCKMASKLEN 24U
#define BLOCKLEN (1U << BLOCKMASKLEN)
#define BLOCKMASK (BLOCKLEN - 1U)

#define OUTPUTSIZE 1000001

int main(void) {
  FILE *fp;
  uint32_t curoffset = 0;
  uint32_t *data;
  uint32_t *outputtag;
  uint64_t *outputtsc;
  struct randstate rstate;

  initGenerator(&rstate);

  seedGenerator(&rstate);

  // Allocate the block
  data = malloc(sizeof(uint32_t) * BLOCKLEN);
  assert(data != NULL);

  outputtsc = malloc(sizeof(uint64_t) * OUTPUTSIZE);
  assert(outputtsc != NULL);

  outputtag = malloc(sizeof(uint32_t) * OUTPUTSIZE);
  assert(outputtag != NULL);

  // Populate the block
  for (size_t i = 0; i < BLOCKLEN; i++) data[i] = randomu32(&rstate);

  fp = fopen("tagout-u32.bin", "w");
  assert(fp != NULL);

  while (1) {
    uint32_t rawloc;
    size_t res;

    for (uint32_t j = 0; j < OUTPUTSIZE; j++) {
      unsigned int dummy;
      uint32_t newData = randomu32(&rstate);
      uint64_t start;
      uint64_t end;

      start = __builtin_ia32_rdtscp(&dummy);
      rawloc = data[curoffset];
      data[curoffset] = newData;
      end = __builtin_ia32_rdtscp(&dummy);

      outputtag[j] = (rawloc & (~BLOCKMASK)) >> BLOCKMASKLEN;
      outputtsc[j] = end - start;
      curoffset = rawloc & BLOCKMASK;
    }

    fwrite(outputtag, sizeof(uint32_t), OUTPUTSIZE, fp);
    res = fwrite(outputtsc, sizeof(uint64_t), OUTPUTSIZE, stdout);
    assert(res == OUTPUTSIZE);
  }
}
