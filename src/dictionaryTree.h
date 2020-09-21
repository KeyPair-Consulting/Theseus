/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef DICTTREE_H
#define DICTTREE_H

#include <stdbool.h>
#include <stdint.h>
#include "enttypes.h"
#include "poolalloc.h"

// These contain the actual hash table (dictionaryEntry)
// 16 bytes (ish) each for 1 byte statData
struct dictionaryPage {
  signedStatData_t curModulus;  // We need to be able to represent k
  bool prefixFound;  // Has the prefix ending at this node been encountered?
  statData_t maxEntry;  // What is the max postfix?
  // The table containing entries. The postfix is present iff count>0.
  struct dictionaryEntry *entries;
};

// Each node in the hash table must identify the symbol
// 24 bytes (ish) each
struct dictionaryEntry {
  statData_t symbol;
  size_t count;
  struct dictionaryPage *branch;
};

bool treeIncrementDict(const statData_t *prior, size_t pLen, struct dictionaryPage *head, size_t k, bool createEntry, bool leafCounts, struct dictionaryPage *prefixLoc, struct memSegment **mempools);
size_t treePredictDict(const statData_t *prior, size_t pLen, statData_t *next, struct dictionaryPage *head, struct dictionaryPage **prefixLoc);
struct dictionaryPage *newDictionaryPage(struct memSegment **mempools);
size_t delDictionary(struct dictionaryPage *head, size_t k, size_t *modulusCount, size_t *occupiedCount, struct memSegment **mempools);
#endif
