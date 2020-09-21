/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef POOLALLOC_H
#define POOLALLOC_H

#include <stddef.h>

#define SEGMENTSIZEBOUND 134217728  // 128MB

struct memSegment {
  char *segmentStart;
  void *nextFree;
  struct memSegment *nextSegment;
  size_t blockSize;
  size_t blockCount;
};

struct memSegment *initPool(size_t bsize, size_t bcount);
size_t delPool(struct memSegment *pool);
void blockFree(void *discard, struct memSegment *pool);
void *blockAlloc(struct memSegment *pool);
#endif
