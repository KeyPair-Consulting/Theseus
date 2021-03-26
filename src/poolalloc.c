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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "globals.h"
#include "poolalloc.h"

/*A simple pool (block) allocator to deal with all the small allocs in the code*/

static void allocSegment(struct memSegment *new) {
  size_t j;
  char *curLoc;
  char *nextLoc;

  assert(new != NULL);

  if (configVerbose > 4) fprintf(stderr, "Allocate a new segment (bsize = %zu)\n", new->blockSize);

  if ((new->segmentStart = malloc(new->blockSize *new->blockCount)) == NULL) {
    perror("Can't allocate data for segment backing");
    exit(EX_OSERR);
  }
  for (j = 0, curLoc = new->segmentStart; j < new->blockCount - 1; j++, curLoc += new->blockSize) {
    // populate the list with pointers to the next location.
    nextLoc = curLoc + new->blockSize;
    memcpy(curLoc, &nextLoc, sizeof(char *));
  }

  // signal the end of the list
  nextLoc = NULL;
  memcpy(curLoc, &nextLoc, sizeof(char *));
}

struct memSegment *initPool(size_t bsize, size_t bcount) {
  size_t slop = bsize % sizeof(void *);
  struct memSegment *startSegment;

  if (configVerbose > 4) fprintf(stderr, "Making a new memory pool (bsize = %zu, bcount = %zu)\n", bsize, bcount);

  if ((startSegment = malloc(sizeof(struct memSegment))) == NULL) {
    perror("Can't allocate initial segment");
    exit(EX_OSERR);
  }

  if (slop != 0) {
    if (configVerbose > 2) fprintf(stderr, "Adjusted allocation block size from %zu to %zu to maintain alignment.\n", bsize, bsize + sizeof(void *) - slop);
    bsize += sizeof(void *) - slop;
  }
  assert((bsize & 0x7) == 0);

  startSegment->blockSize = bsize;
  startSegment->blockCount = bcount;
  startSegment->nextSegment = NULL;
  allocSegment(startSegment);
  startSegment->nextFree = startSegment->segmentStart;
  return (startSegment);
}

size_t delPool(struct memSegment *pool) {
  struct memSegment *next;
  size_t blockCount = 0;
  size_t curMem;
  size_t bsize;

  assert(pool != NULL);
  bsize = pool->blockSize;

  while (pool != NULL) {
    next = pool->nextSegment;
    blockCount += pool->blockCount;
    free(pool->segmentStart);
    free(pool);
    pool = next;
  }

  curMem = bsize * blockCount;
  if (configVerbose > 4) fprintf(stderr, "Block allocator (bsize = %zu, bcount = %zu) takes %zu bytes\n", bsize, blockCount, curMem);

  return (curMem);
}

void blockFree(void *discard, struct memSegment *pool) {
  char *next;

  assert(pool != NULL);

  next = pool->nextFree;

  if (configVerbose > 7) fprintf(stderr, "Free a block (bsize = %zu)\n", pool->blockSize);
  memcpy(discard, &next, sizeof(char *));
  pool->nextFree = discard;
}

void *blockAlloc(struct memSegment *pool) {
  char *out;

  assert(pool != NULL);

  if (configVerbose > 7) fprintf(stderr, "Allocate a new block (bsize = %zu)\n", pool->blockSize);
  if (pool->nextFree == NULL) {
    // We've run out of space in the current segment(s). Make a new segment.
    struct memSegment *newSegment;
    struct memSegment *endSegment = pool;

    // Go to the end of the segment list
    while (endSegment->nextSegment != NULL) {
      endSegment = endSegment->nextSegment;
    }

    // Start on a new segment.
    if ((newSegment = malloc(sizeof(struct memSegment))) == NULL) {
      perror("Can't allocate initial segment");
      exit(EX_OSERR);
    }

    endSegment->nextSegment = newSegment;
    newSegment->blockSize = pool->blockSize;
    // Double the size each allocation, up to a bound
    newSegment->blockCount = (endSegment->blockCount) << 1;
    if (newSegment->blockCount * newSegment->blockSize > SEGMENTSIZEBOUND) {
      newSegment->blockCount = endSegment->blockCount;
    }
    newSegment->nextSegment = NULL;

    if (configVerbose > 4) fprintf(stderr, "Expanding the number of pools (bsize = %zu, bcount = %zu)\n", pool->blockSize, newSegment->blockCount);

    allocSegment(newSegment);
    // Only the head segment keeps track of the nextFree

    pool->nextFree = newSegment->segmentStart;
  }

  assert(pool->nextFree != NULL);

  out = pool->nextFree;

  // Copy the pointer to the new next element
  memcpy(&(pool->nextFree), pool->nextFree, sizeof(char *));

  // Setup the block for use
  memset(out, 0, pool->blockSize);

  return (out);
}
