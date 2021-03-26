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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "dictionaryTree.h"
#include "globals.h"
#include "hashmodulus.h"
#include "poolalloc.h"

/* Implements a k-tree, where each node uses a hash table with an unconventional collision
 * resolution.  In the event of a hash collision, the hash table is expanded until there
 * isn't a collision. After a fixed number of expansions, the hash table is expanded
 * to a look up table (equivalent to setting the hash modulus to k)
 * This makes sense due to the expected distribution of these counts
 * in the random case. Pre-allocating k-length look up tables would be simpler, but
 * _most_ of these will never need nearly that much storage
 */

// A function to check if the dictionary entry has yet been initialized
static bool isDictionaryEntryInit(const struct dictionaryEntry *in) {
  return ((in != NULL) && ((in->branch != NULL) || (in->count > 0)));
}

// recursively clean up the entire dictionary
// The recursive pattern isn't insane here, as the depth is limited
size_t delDictionary(struct dictionaryPage *head, size_t k, size_t *modulusCount, size_t *occupiedCount, struct memSegment **mempools) {
  size_t c;
  size_t modIndex;
  const signedStatData_t *modPtr;
  size_t pagesDeleted = 0;

  if (head == NULL) {
    return 0;
  }

  if ((size_t)head->curModulus == k) {
    modIndex = MODULUSCOUNT - 1;
  } else {
    modPtr = hashModulus;
    while ((*modPtr != head->curModulus) && (*modPtr != 0)) {
      modPtr++;
    }

    assert(head->curModulus == *modPtr);

    assert(modPtr >= hashModulus);
    modIndex = (size_t)(modPtr - hashModulus);
  }

  modulusCount[modIndex]++;

  c = 0;
  for (signedStatData_t j = 0; j < head->curModulus; j++) {
    if (isDictionaryEntryInit(head->entries + j)) {
      c++;

      if ((head->entries[j]).branch != NULL) {
        pagesDeleted += delDictionary((head->entries[j]).branch, k, modulusCount, occupiedCount, mempools);
        (head->entries[j]).branch = NULL;
      }
    }
  }

  assert(mempools[modIndex] != NULL);
  blockFree(head->entries, mempools[modIndex]);
  head->entries = NULL;

  assert(mempools[MODULUSCOUNT] != NULL);
  blockFree(head, mempools[MODULUSCOUNT]);
  head = NULL;
  occupiedCount[modIndex] += c;
  return pagesDeleted + 1;
}

// Locates the dictionary entry (within curPage) associated with a particular (prefix or postfix) symbol
static struct dictionaryEntry *getDictionaryEntry(struct dictionaryPage *curPage, statData_t symbol) {
  struct dictionaryEntry *curEntry;

  curEntry = curPage->entries + (((signedStatData_t)symbol) % curPage->curModulus);
  if (curEntry->symbol == symbol) {
    return (curEntry);
  } else {
    return NULL;
  }
}

// Given an initialized prefix dictionary page, this returns the number of times this symbol has been encountered.
// Once the prefix dictionary page is marked as initialized, it must necessarily have such an entry
static size_t getMaxPageCount(struct dictionaryPage *curPage) {
  struct dictionaryEntry *curEntry;

  assert(curPage != NULL);
  assert(curPage->prefixFound);

  curEntry = getDictionaryEntry(curPage, curPage->maxEntry);
  assert(curEntry != NULL);
  return (curEntry->count);
}

// Locates the dictionaryPage associated with a particular initialized prefix (data[0], ..., data[pLen-1])
// head points to the dictionary page associated with the empty prefix (the root of the tree).
// If the path to that prefix doesn't exist, return NULL
// otherwise, return the found page
static struct dictionaryPage *getPrefixPage(struct dictionaryPage *head, const statData_t *data, size_t pLen) {
  struct dictionaryEntry *curEntry;

  for (size_t i = 0; (head != NULL) && (i < pLen); i++) {
    curEntry = getDictionaryEntry(head, data[i]);
    if (curEntry == NULL) return NULL;
    head = curEntry->branch;
  }

  return head;
}

/*Initializes a dictionary page, including the dictionary entry storage */
struct dictionaryPage *newDictionaryPage(struct memSegment **mempools) {
  struct dictionaryPage *out;

  assert(mempools[MODULUSCOUNT] != NULL);
  out = blockAlloc(mempools[MODULUSCOUNT]);

  out->curModulus = *hashModulus;
  out->prefixFound = false;

  assert(out->curModulus > 0);

  assert(mempools[0] != NULL);
  out->entries = blockAlloc(mempools[0]);

  return out;
}

// This is called in the event of a collision within the dictionary entry hash table.
// We don't do probing of any sort, so instead, we just expand the hash table until there isn't a collision.
// This necessarily terminates, because eventually the table just becomes a direct look up table.
static void expandPage(struct dictionaryPage *in, statData_t newSymbol, size_t k, struct memSegment **mempools) {
  struct dictionaryEntry *newEntries = NULL;
  signedStatData_t trialModulus;
  bool haveCollision;
  size_t newloc;
  size_t initModIndex;

  assert(k - 1 <= STATDATA_MAX);

  // Make new pages using the creation call
  assert(in != NULL);
  // There is presently a collision with newSymbol, so the current modulus isn't k
  assert(((size_t)(in->curModulus)) != k);
  assert(k - 1 <= STATDATA_MAX);

  // find the index of the current modulus
  initModIndex = 0;
  while ((hashModulus[initModIndex] < in->curModulus) && (hashModulus[initModIndex] != 0)) {
    initModIndex++;
  }
  assert(hashModulus[initModIndex] != 0);
  assert(hashModulus[initModIndex] == in->curModulus);

  haveCollision = true;
  trialModulus = in->curModulus;
  assert(getDictionaryEntry(in, newSymbol) == NULL);

  while (haveCollision) {
    size_t modIndex;

    // Look for the next modulus.
    // This is a linear search, but the list is only a few long, and should fit within a single cache line, so it should be quick.
    modIndex = 0;
    while ((hashModulus[modIndex] <= trialModulus) && (hashModulus[modIndex] > 0)) {
      modIndex++;
    }

    if ((hashModulus[modIndex] == 0) || ((size_t)hashModulus[modIndex] >= k)) {
      trialModulus = (signedStatData_t)k;
      modIndex = MODULUSCOUNT - 1;
    } else {
      trialModulus = hashModulus[modIndex];
    }

    assert(modIndex > 0);
    assert((trialModulus > 0) && ((size_t)trialModulus <= k));

    // Allocate the space for the new dictionary entries
    assert(mempools[modIndex] != NULL);
    newEntries = blockAlloc(mempools[modIndex]);

    // Populate the new space with the existing data
    haveCollision = false;

    for (signedStatData_t j = 0; j < in->curModulus; j++) {
      if (isDictionaryEntryInit(in->entries + j)) {
        newloc = ((size_t)(in->entries[j].symbol)) % ((size_t)trialModulus);
        if (!isDictionaryEntryInit(newEntries + newloc)) {
          memcpy(newEntries + newloc, in->entries + j, sizeof(struct dictionaryEntry));
        } else {
          haveCollision = true;
          if (configVerbose > 5) fprintf(stderr, "Found collision after expanding tree hash table.\n");
          assert((size_t)trialModulus != k);
          break;
        }
      }
    }

    // Is the symbol we are trying to add a collision?
    haveCollision = haveCollision || isDictionaryEntryInit(newEntries + (((size_t)newSymbol) % ((size_t)trialModulus)));

    if (haveCollision) {
      assert((size_t)trialModulus < k);
      assert(mempools[modIndex] != NULL);
      blockFree(newEntries, mempools[modIndex]);
      newEntries = NULL;
    }
  }

  assert(mempools[initModIndex] != NULL);
  blockFree(in->entries, mempools[initModIndex]);
  in->entries = newEntries;
  in->curModulus = trialModulus;
}

// Add a particular symbol to the hash table, and expand the hash table if necessary.
// Note, THIS DOES NOT INITIALIZE THE ENTRY. This helper function only puts the value in the hash table
//(expanding the hash table as necessary), and returns a pointer to the location that still needs to be
// initialized (depending on its status as a prefix entry, a postfix entry, or both).
static struct dictionaryEntry *addDictionaryEntry(struct dictionaryPage *curPage, statData_t symbol, size_t k, struct memSegment **mempools) {
  struct dictionaryEntry *cur;

  cur = curPage->entries + (((signedStatData_t)symbol) % curPage->curModulus);

  if (isDictionaryEntryInit(cur)) {
    // We shouldn't try to add a symbol that is already here...
    assert(cur->symbol != symbol);
    expandPage(curPage, symbol, k, mempools);

    // expandPage verifies that we can add this element.
    cur = curPage->entries + (((signedStatData_t)symbol) % curPage->curModulus);
  }

  // Set the symbol
  cur->symbol = symbol;

  return cur;
}

// If allowed, Creates the dictionaryPage associated with a particular prefix (data[0], ..., data[pLen-1])
// head points to the dictionary page associated with the empty prefix (the root of the tree).
// If the path to that prefix doesn't exist, and we aren't allowed to create it, return NULL
// otherwise, return the found / created page
static struct dictionaryPage *makePrefixPage(struct dictionaryPage *head, const statData_t *data, size_t pLen, size_t k, bool createEntry, bool *newPrefixNeeded, struct memSegment **mempools) {
  struct dictionaryEntry *curEntry;
  struct dictionaryPage *curPage;
  struct dictionaryPage *lastPage = NULL;
  size_t i;

  assert(pLen > 0);
  assert(data != NULL);
  assert(head != NULL);
  assert(newPrefixNeeded != NULL);

  // We will go through this loop at least once, so lastPage will be initialized
  for (i = 0, curPage = head; (curPage != NULL) && (i < pLen); i++) {
    lastPage = curPage;
    curEntry = getDictionaryEntry(curPage, data[i]);
    if (curEntry == NULL) {
      // Here, we have a page, but can't locate the relevant entry
      curPage = NULL;
    } else {
      curPage = curEntry->branch;
    }
  }

  if ((curPage != NULL)) {
    // We located the prefix page. Had we seen it before?
    if (curPage->prefixFound) {
      // the prefix was previously encountered. Return it.
      *newPrefixNeeded = false;
      return curPage;
    } else if (createEntry) {
      *newPrefixNeeded = true;
      // We've never seen this particular prefix before, but we can "create" it.
      return curPage;
    } else {
      *newPrefixNeeded = true;
      // We've never seen this particular prefix before, and we can't "create" it.
      return NULL;
    }
  } else {
    // We went some way in, but ran out of prefix tree.
    *newPrefixNeeded = true;
    if (createEntry) {
      // We are allowed to add this prefix to the tree
      assert(i > 0);
      // Go back up to one look up
      // Complete the prefix
      for (i = i - 1, curPage = lastPage; i < pLen; i++) {
        curEntry = getDictionaryEntry(curPage, data[i]);
        if (curEntry == NULL) curEntry = addDictionaryEntry(curPage, data[i], k, mempools);
        assert(curEntry != NULL);
        curEntry->branch = newDictionaryPage(mempools);
        curPage = curEntry->branch;
      }

      // curPage is now the page corresponding to this prefix
      return curPage;
    } else {
      return NULL;
    }
  }
}

// CreateEntry tells us if we can create postfix entries.
// The return is the number of times this postfix has been entered into the hash table
static size_t incrementDictionaryEntry(statData_t symbol, struct dictionaryPage *curPage, size_t k, bool createEntry, struct memSegment **mempools) {
  struct dictionaryEntry *curEntry;

  assert(curPage != NULL);

  // First, find the right symbol
  curEntry = getDictionaryEntry(curPage, symbol);
  if (curEntry != NULL) {
    if ((curEntry->count > 0) || createEntry) {
      // The right symbol is already here, or we are allowed to create it.
      (curEntry->count)++;
      return (curEntry->count);
    } else {
      // The symbol was present as part of a prefix, we've never seen it,
      // and we aren't allowed to create it as a postfix
      return (0);
    }
  } else {
    // The symbol isn't yet in the compressed hash table
    if (createEntry) {
      //... but I can create it
      curEntry = addDictionaryEntry(curPage, symbol, k, mempools);
      curEntry->count = 1;
      curEntry->symbol = symbol;
      return (1);
    } else {
      //... and I can't create it
      return (0);
    }
  }
}

// createEntry tells us if we can create new "counted" entries.
// leafCounts tells us if we should treat leaf node creation as creation.
// If "leafCounts" is false and the prefix exists and is initialized, then postfix entries are always created irrespective of the "createEntry" setting.
// the return indicates if branch / leaf nodes (if leafCounts is true) would have been created (or were created, if that was allowed)
// The first pLen elements [0, ..., pLen-1] of the statData_t string "prior" are the prefix string
// prior[pLen] is the postfix that we are attempting to increment
// prefixLoc provides a way of passing in the relevant location within the data structure (rather than having to look it up)
// incrementDict() is the consumer of such cached information, which should have been set in the prior loop iteration's predictDict().
bool treeIncrementDict(const statData_t *prior, size_t pLen, struct dictionaryPage *head, size_t k, bool createEntry, bool leafCounts, struct dictionaryPage *prefixLoc, struct memSegment **mempools) {
  statData_t newData;
  bool newPrefixNeeded = false;
  struct dictionaryPage *prefixPage;
  size_t curCount;

  assert(prior != NULL);
  assert(head != NULL);

  newData = prior[pLen];

  if (prefixLoc == NULL) {
    // find the right prefix
    prefixPage = makePrefixPage(head, prior, pLen, k, createEntry, &newPrefixNeeded, mempools);
    if (prefixPage == NULL) {
      // We weren't allowed to create the prefix page
      return (true);
    } else {
      if ((configVerbose > 7) && newPrefixNeeded) {
        fprintf(stderr, "Adding %zu-length string: ", pLen);
        for (size_t j = 0; j < pLen - 1; j++) fprintf(stderr, "%02x:", prior[j]);
        fprintf(stderr, "%02x\n", prior[pLen - 1]);
      }
    }
  } else {
#ifdef SLOWCHECKS
    // If there is a cached location, does it refer to the correct location?
    assert((prefixLoc == NULL) || (getPrefixPage(head, prior, pLen) == prefixLoc));
#endif

    prefixPage = prefixLoc;

    if (!prefixPage->prefixFound) {
      if (!createEntry) {
        return true;  // We haven't encountered this prefix before, and we can't create it.
      } else {
        newPrefixNeeded = true;
        if ((configVerbose > 7) && newPrefixNeeded) {
          fprintf(stderr, "Adding %zu-length string: ", pLen);
          for (size_t j = 0; j < pLen - 1; j++) fprintf(stderr, "%02x:", prior[j]);
          fprintf(stderr, "%02x\n", prior[pLen - 1]);
        }
      }
    }
  }

  // At this point, prefixPage points to the correct place, and we can try to create the necessary dictionary entry
  curCount = incrementDictionaryEntry(newData, prefixPage, k, createEntry || !leafCounts, mempools);
  if (curCount == 1) {
    if ((configVerbose > 7)) {
      fprintf(stderr, "Incrementing %zu-length prefix: ", pLen);
      for (size_t j = 0; j < pLen; j++) fprintf(stderr, "%02x:", prior[j]);
      fprintf(stderr, ":%02x -> 1\n", newData);
    }
    // This should only occur when we can create new leaves
    assert(createEntry || !leafCounts);
    if (prefixPage->prefixFound) {
      // This prefix page was already initialized, but this is a new symbol.
      if ((getMaxPageCount(prefixPage) == 1) && (prefixPage->maxEntry < newData)) {
        prefixPage->maxEntry = newData;
      }
    } else {
      // This page wasn't initialized. Do so.
      prefixPage->maxEntry = newData;
      prefixPage->prefixFound = true;
    }
  } else if (curCount > 1) {
    size_t curMaxCount = getMaxPageCount(prefixPage);
    if ((curMaxCount < curCount) || ((curMaxCount == curCount) && (prefixPage->maxEntry < newData))) {
      prefixPage->maxEntry = newData;
    }
    if ((configVerbose > 7)) {
      fprintf(stderr, "Incrementing %zu-length prefix: ", pLen);
      for (size_t j = 0; j < pLen; j++) fprintf(stderr, "%02x:", prior[j]);
      fprintf(stderr, ":%02x -> %zu\n", newData, curCount);
    }
  }

  // At this stage, we've already returned in any instance where we weren't allowed to create a prefix (branch)
  // So the only times newCount == 0 are when we weren't allowed to create new postfix entries (leaves).
  assert((curCount > 0) || (!createEntry && leafCounts));
  if (leafCounts) {
    return (curCount <= 1);
  } else {
    return newPrefixNeeded;
  }
}

// predictDict() is expected to populate the prefix string cache. The next loop iteration's incrementDict() is the consumer of such cached information.
// If all the branches exist, extract the most likely symbol.
// The first pLen elements [0, ..., pLen-1] of the statData_t string "prior" are the symbols that lead up to this
//*next is used to pass back the prediction
// Returns the number of these "best" elements that have been observed
// store the predicted symbol in *next.
size_t treePredictDict(const statData_t *prior, size_t pLen, statData_t *next, struct dictionaryPage *head, struct dictionaryPage **prefixLoc) {
  struct dictionaryEntry *curDictionaryEntry;

  assert(prior != NULL);
  assert(next != NULL);
  assert(head != NULL);
  assert(prefixLoc != NULL);

  // First grab the relevant prefix page
  head = getPrefixPage(head, prior, pLen);
  // Save the location
  *prefixLoc = head;
  if ((head == NULL) || (!(head->prefixFound))) {
    if ((configVerbose > 7)) {
      fprintf(stderr, "Failed to predict %zu-length string: ", pLen);
      for (size_t j = 0; j < pLen - 1; j++) fprintf(stderr, "%02x:", prior[j]);
      fprintf(stderr, "%02x due to lack of prefix\n", prior[pLen - 1]);
    }
    return 0;
  } else
    *next = head->maxEntry;

  // We already know that maxEntry is set, and the referenced symbol should exist
  curDictionaryEntry = getDictionaryEntry(head, head->maxEntry);
  assert((curDictionaryEntry != NULL) && (curDictionaryEntry->count > 0));

  if ((configVerbose > 7)) {
    fprintf(stderr, "Predict %zu-length string: ", pLen);
    for (size_t j = 0; j < pLen; j++) fprintf(stderr, "%02x:", prior[j]);
    fprintf(stderr, ":%02x -> %zu\n", *next, curDictionaryEntry->count);
  }

  return (curDictionaryEntry->count);
}
