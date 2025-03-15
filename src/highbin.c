/* This file is part of the Theseus distribution.
 * Copyright 2020-2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <fenv.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "fancymath.h"
#include "globals-inst.h"
#include "precision.h"
#include "rbt_misc.h"
#include "red_black_tree.h"

#define LEFTENDPOINTFLAG 0x02
#define RIGHTENDPOINTFLAG 0x01

// Some notes on data structures:
// if we have k symbols, symbolTable is (at least) a k x 3 matrix of uint32_t values
// symbolTable[3j+0] is the untranslated (raw) data
// symbolTable[3j+1] is the population of that data element data
// symbolTable[3j+2] is for flags denoting interval endpoints

// If we have k intervals, interval arrays are stored as:
// intervals[3j+0] is the untranslated (raw) data interval start (inclusive)
// intervals[3j+1] is the untranslated (raw) data interval end (inclusive)
// intervals[3j+2] is the population of that interval

struct intervalNode {
  uint64_t treeKey;
  struct intervalNode *nextNode;
  struct intervalNode *priorNode;

  uint32_t intervalStart;  // The first translated
  uint32_t intervalEnd;
  uint32_t intervalPop;  // The population of elements in this interval
};

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "highbin inputfile outputBits\n");
  fprintf(stderr, "Attempts to bin input symbols into bins with equal numbers of adjacent samples.\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output is to stdout, and is " STATDATA_STRING " ints\n");
  exit(EX_USAGE);
}

static int uintcompare(const void *in1, const void *in2) {
  const uint32_t *left;
  const uint32_t *right;

  left = in1;
  right = in2;

  if (*left < *right) {
    return (-1);
  } else if (*left > *right) {
    return (1);
  } else {
    return (0);
  }
}

// Make sure that the intervals are well ordered.
static int intervalCompare(const void *in1, const void *in2) {
  const uint32_t *left;
  const uint32_t *right;

  assert(in1 != NULL);
  assert(in2 != NULL);

  left = in1;
  right = in2;

  /*Check to see that the individual intervals are well formed.*/
  assert(*left <= *(left + 1));
  assert(*right <= *(right + 1));

  if ((*left < *right) && ((*left < *(right + 1))) && (*(left + 1) < *right) && (*(left + 1) < *(right + 1))) {
    return (-1);
  } else if ((*left > *right) && ((*left > *(right + 1))) && (*(left + 1) > *right) && (*(left + 1) > *(right + 1))) {
    return (1);
  } else if ((*left == *right) && (*(left + 1) == *(right + 1))) {
    return (0);
  } else {
    fprintf(stderr, "Incomparable intervals detected. No reason for this to occur...\n");
    fprintf(stderr, "Compared [%u, %u] with [%u, %u]\n", *left, *(left + 1), *right, *(right + 1));
    exit(EX_DATAERR);
  }
}

static int intervalMembershipCompare(const void *vkey, const void *elem) {
  const uint32_t *key;
  const uint32_t *interval;

  key = vkey;
  interval = elem;

  if (*key < *interval) {
    return (-1);
  } else if (*key > *(interval + 1)) {
    return (1);
  } else {
    return (0);
  }
}

/*Tests:
 * buckets == targetBuckets
 * buckets > targetBuckets
 * buckets < targetBuckets
 */
static double chiSquareScore(uint32_t *intervals, size_t buckets, double targetPopulation, size_t targetBuckets) {
  double score = 0.0;
  size_t i;
  size_t normalBuckets;

  assert(buckets > 0);
  assert(targetBuckets > 0);
  normalBuckets = (buckets <= targetBuckets) ? buckets : targetBuckets;  // normalBuckets = min(buckets, targetBuckets)
  assert(normalBuckets <= buckets);

  // Process those buckets that are expected
  for (i = 0; i < normalBuckets; i++) {
    assert(intervals[3 * i + 2] > 0);
    score += square(((double)(intervals[3 * i + 2])) - targetPopulation);
  }

  // In the event that we have more buckets than was desired for whatever reason...
  // Here, the expectation is is 0
  for (i = normalBuckets; i < buckets; i++) {
    score += square((double)intervals[3 * i + 2]);
  }

  // In the event that we have fewer buckets than was desired.
  if (buckets < targetBuckets) {
    /*Fix up for empty buckets*/
    score += ((double)(targetBuckets - buckets)) * square((double)targetPopulation);
  }

  return (score / targetPopulation);
}

static int treeIndexCompare(const void *ap, const void *bp) {
  const uint64_t a = *((const uint64_t *)ap);
  const uint64_t b = *((const uint64_t *)bp);

  if (a > b) {
    return 1;
  } else if (a < b) {
    return -1;
  } else {
    return 0;
  }
}

static void updateTreeKey(struct intervalNode *currentInterval) {
  uint64_t newCombinedPopulation;
  uint64_t intervalStart;

  assert(currentInterval != NULL);
  assert(currentInterval->intervalStart <= 0xFFFFFFFF);

  intervalStart = currentInterval->intervalStart;

  if (currentInterval->nextNode != NULL) {
    newCombinedPopulation = currentInterval->intervalPop + currentInterval->nextNode->intervalPop;
  } else {
    newCombinedPopulation = UINT_MAX;
  }

  assert(newCombinedPopulation <= 0xFFFFFFFF);

  currentInterval->treeKey = ((newCombinedPopulation << 32) | intervalStart);
}

static rb_red_blk_tree *initIntervalSearch(uint32_t *symbolTable, size_t symbolCount, struct intervalNode **outhead) {
  struct intervalNode *currentInterval;
  struct intervalNode *lastInterval = NULL;
  rb_red_blk_tree *tree;
  size_t i;

  assert(symbolCount > 0);

  tree = RBTreeCreate(treeIndexCompare, NullFunction, NullFunction);
  assert(tree != NULL);

  fprintf(stderr, "BBGG: Inserting symbols into RBT.\n");
  if ((currentInterval = malloc(sizeof(struct intervalNode))) == NULL) {
    perror("Can't allocate array for current interval");
    exit(EX_OSERR);
  }

  currentInterval->intervalStart = symbolTable[0];
  currentInterval->intervalEnd = symbolTable[0];
  currentInterval->intervalPop = symbolTable[1];
  currentInterval->nextNode = NULL;
  currentInterval->priorNode = NULL;

  *outhead = currentInterval;

  lastInterval = currentInterval;

  for (i = 1; i < symbolCount; i++) {
    if ((currentInterval = malloc(sizeof(struct intervalNode))) == NULL) {
      perror("Can't allocate array for current interval");
      exit(EX_OSERR);
    }

    currentInterval->intervalStart = symbolTable[3 * i];
    currentInterval->intervalEnd = symbolTable[3 * i];
    currentInterval->intervalPop = symbolTable[3 * i + 1];

    currentInterval->priorNode = lastInterval;
    lastInterval->nextNode = currentInterval;

    updateTreeKey(lastInterval);
    RBTreeInsert(tree, &(lastInterval->treeKey), lastInterval);
    lastInterval = currentInterval;
  }

  /*get the last symbol into a special interval*/
  currentInterval->nextNode = NULL;

  updateTreeKey(currentInterval);
  RBTreeInsert(tree, &(currentInterval->treeKey), currentInterval);

  return (tree);
}

/*test cases:
 * every symbol is different
 * Many common symbols, but enough to distinct buckets
 * many common symbols, but not enough for distinct buckets
 * one symbol
 */
static double bucketByGreedyGrouping(uint32_t *symbolTable, size_t symbolCount, /*@out@*/ uint32_t *intervals, size_t *buckets, double targetPopulation) {
  size_t curBuckets;
  size_t i;
  rb_red_blk_tree *tree;
  rb_red_blk_node *curNode;
  rb_red_blk_node *nextNode;
  rb_red_blk_node *priorNode;
  struct intervalNode *currentInterval;
  struct intervalNode *lastInterval;
  struct intervalNode *nextInterval;
  struct intervalNode *priorInterval;
  struct intervalNode *intervalHead;
  uint64_t nextIndex;
  uint64_t priorIndex;
  size_t targetBuckets;

  assert(buckets != NULL);
  assert(*buckets > 0);
  assert(symbolTable != NULL);
  assert(intervals != NULL);

  targetBuckets = *buckets;

  /*Clear out any endpoints marked*/
  for (i = 0; i < symbolCount; i++) {
    symbolTable[i * 3 + 2] = 0;
  }

  tree = initIntervalSearch(symbolTable, symbolCount, &intervalHead);
  assert(tree != NULL);
  assert(intervalHead != NULL);

  fprintf(stderr, "BBGG: Combining small adjacent interval pairs.\n");

  for (curBuckets = symbolCount; curBuckets > *buckets; curBuckets--) {
    curNode = RBMinQuery(tree);
    assert(curNode != NULL);
    currentInterval = curNode->info;

    /*We shouldn't get the end node here*/
    assert(currentInterval->nextNode != NULL);

    nextIndex = (currentInterval->nextNode)->treeKey;
    nextNode = RBExactQuery(tree, &nextIndex);
    assert(nextNode != NULL);
    nextInterval = nextNode->info;

    currentInterval->intervalEnd = nextInterval->intervalEnd;
    currentInterval->intervalPop += nextInterval->intervalPop;
    currentInterval->nextNode = nextInterval->nextNode;

    if (currentInterval->nextNode != NULL) {
      currentInterval->nextNode->priorNode = currentInterval;
    }

    RBDelete(tree, nextNode);
    RBDelete(tree, curNode);

    updateTreeKey(currentInterval);
    RBTreeInsert(tree, &(currentInterval->treeKey), currentInterval);

    if (currentInterval->priorNode != NULL) {
      priorIndex = currentInterval->priorNode->treeKey;
      priorNode = RBExactQuery(tree, &priorIndex);
      assert(priorNode != NULL);

      priorInterval = priorNode->info;
      assert(priorInterval != NULL);

      RBDelete(tree, priorNode);

      updateTreeKey(priorInterval);
      RBTreeInsert(tree, &(priorInterval->treeKey), priorInterval);
    }

    assert(nextInterval != NULL);
    free(nextInterval);
    nextInterval = NULL;
  }

  fprintf(stderr, "BBGG: Extracting intervals.\n");

  currentInterval = intervalHead;
  i = 0;
  while (currentInterval != NULL) {
    intervals[3 * i] = currentInterval->intervalStart;
    intervals[3 * i + 1] = currentInterval->intervalEnd;
    intervals[3 * i + 2] = currentInterval->intervalPop;

    curNode = RBExactQuery(tree, &(currentInterval->treeKey));
    RBDelete(tree, curNode);

    lastInterval = currentInterval;
    currentInterval = currentInterval->nextNode;
    assert(lastInterval != NULL);
    free(lastInterval);
    lastInterval = NULL;
    i++;
  }

  /*The tree should be empty. Destroy it*/
  assert(NULL == RBMinQuery(tree));
  RBTreeDestroy(tree);
  tree = NULL;

  *buckets = curBuckets;

  return (chiSquareScore(intervals, curBuckets, targetPopulation, targetBuckets));
}

static double bucketByOrderedGreedy(uint32_t *symbolTable, size_t symbolCount, uint32_t *intervals, size_t *buckets, double targetPopulation) {
  size_t i;
  size_t curSymbol;
  size_t localTargetPopulation;
  size_t dataLen;
  size_t roughDataLen;
  size_t roughSymbols;
  size_t targetBuckets;

  targetBuckets = *buckets;
  dataLen = 0;
  roughDataLen = 0;
  roughSymbols = 0;
  for (i = 0; i < symbolCount; i++) {
    dataLen += symbolTable[i * 3 + 1];
    if (symbolTable[i * 3 + 1] > targetPopulation) {
      roughDataLen += symbolTable[i * 3 + 1];
      roughSymbols++;
    }
  }

  if ((roughSymbols > 0) && (symbolCount > roughSymbols)) {
    // This is the best we can do after excluding symbols that are too large.
    localTargetPopulation = (uint32_t)floor(((double)(dataLen - roughDataLen)) / ((double)(targetBuckets - roughSymbols)));
    fprintf(stderr, "BBOG: Found %zu over-numerous symbols (accounting for %zu samples). New target is %zu\n", roughSymbols, roughDataLen, localTargetPopulation);
  } else {
    localTargetPopulation = (uint32_t)floor(targetPopulation);
    fprintf(stderr, "BBOG: Found %zu over-numerous symbols\n", roughSymbols);
  }

  // Populate this interval by adding one element at a time until we exceed the localTargetPopulation
  curSymbol = 0;
  for (i = 0; (i < (*buckets - 1)) && (curSymbol < symbolCount); i++) {
    intervals[3 * i] = symbolTable[3 * curSymbol];
    intervals[3 * i + 1] = symbolTable[3 * curSymbol];
    intervals[3 * i + 2] = symbolTable[3 * curSymbol + 1];
    curSymbol++;

    // Add as many as we can fit while still not exceeding the localTargetPopulation
    while ((curSymbol < symbolCount) && ((intervals[3 * i + 2] + symbolTable[3 * curSymbol + 1]) <= localTargetPopulation)) {
      intervals[3 * i + 1] = symbolTable[3 * curSymbol];
      intervals[3 * i + 2] += symbolTable[3 * curSymbol + 1];
      curSymbol++;
    }
  }

  if (curSymbol == symbolCount) {
    /*Already dealt with the last symbol, but we still have at least one bucket left to fill...*/
    *buckets = i;
  } else {
    /*We have at least one symbol left, and one last bucket to fill...*/
    /*Fill the last bucket...*/
    intervals[3 * i] = symbolTable[3 * curSymbol];
    intervals[3 * i + 1] = symbolTable[3 * curSymbol];
    intervals[3 * i + 2] = symbolTable[3 * curSymbol + 1];
    curSymbol++;
    while (curSymbol < symbolCount) {
      intervals[3 * i + 1] = symbolTable[3 * curSymbol];
      intervals[3 * i + 2] += symbolTable[3 * curSymbol + 1];
      curSymbol++;
    }
    *buckets = i + 1;
  }

  return (chiSquareScore(intervals, *buckets, targetPopulation, targetBuckets));
}

/*This function takes single interval, and looks for a good place to split it
 *(in a way that minimizes the resulting chi square sum)
 *It returns 1 if a split occurred, and 0 otherwise
 *This operates on the symbolTable, and the left and right endpoints are indexes.
 */
static size_t medianSplit(uint32_t *symbolTable, size_t leftEndpoint, size_t rightEndpoint) {
  size_t intervalCount;
  size_t fullIntervalCount;
  size_t intervalTarget;
  size_t i;

  /*If the right and left endpoint of this interval are equal, we can't do anything to split it*/
  if (leftEndpoint == rightEndpoint) {
    return 0;
  }

  /*Count the number of samples are in this closed interval*/
  fullIntervalCount = 0;
  for (i = leftEndpoint; i <= rightEndpoint; i++) {
    fullIntervalCount += symbolTable[3 * i + 1];
  }

  /*We're looking for the median, so half should be on each side*/
  /* This is the same as intervalTarget /= 2;*/
  intervalTarget = fullIntervalCount >> 1;

  /*We necessarily include the first symbol within the first resulting interval*/
  intervalCount = symbolTable[3 * leftEndpoint + 1];
  /* Add until:
   *  We've added the right endpoint
   *  adding the next interval puts us over the target
   */
  for (i = leftEndpoint + 1; (i <= rightEndpoint) && ((intervalCount + symbolTable[3 * i + 1]) <= intervalTarget); i++) {
    intervalCount += symbolTable[3 * i + 1];
  }

  /* if we haven't added the right endpoint, check if we should do so
   *
   */
  if ((i <= rightEndpoint) && ((2 * intervalCount + symbolTable[3 * i + 1]) <= fullIntervalCount)) {
    /*It is better to include the current value*/
    symbolTable[3 * i + 2] |= RIGHTENDPOINTFLAG;
    symbolTable[3 * (i + 1) + 2] |= LEFTENDPOINTFLAG;
  } else {
    symbolTable[3 * (i - 1) + 2] |= RIGHTENDPOINTFLAG;
    symbolTable[3 * i + 2] |= LEFTENDPOINTFLAG;
  }

  return 1;
}

/*Setup the interval table based on the intervals noted in the symbol table*/
static size_t extractIntervals(uint32_t *symbolTable, size_t symbolCount, uint32_t *intervals) {
  size_t currentLeftEndpoint;
  size_t currentRightEndpoint;
  uint32_t currentIntervalCount;
  size_t i, j;

  /*Fill out the interval table*/
  currentLeftEndpoint = SIZE_MAX;
  currentRightEndpoint = SIZE_MAX;
  j = 0;
  currentIntervalCount = 0;

  for (i = 0; i < symbolCount; i++) {
    if ((symbolTable[i * 3 + 2] & LEFTENDPOINTFLAG) != 0) {
      assert(currentLeftEndpoint == SIZE_MAX);
      currentLeftEndpoint = i;
      currentIntervalCount = 0;
    }

    /*At this point, the left endpoint should be established*/
    assert(currentLeftEndpoint != SIZE_MAX);
    currentIntervalCount += symbolTable[i * 3 + 1];

    if ((symbolTable[i * 3 + 2] & RIGHTENDPOINTFLAG) != 0) {
      // End the interval
      assert(currentRightEndpoint == SIZE_MAX);
      currentRightEndpoint = i;
      intervals[j * 3] = symbolTable[currentLeftEndpoint * 3];
      intervals[j * 3 + 1] = symbolTable[currentRightEndpoint * 3];
      intervals[j * 3 + 2] = currentIntervalCount;
      currentRightEndpoint = SIZE_MAX;
      currentLeftEndpoint = SIZE_MAX;
      currentIntervalCount = 0;
      j++;
    }
  }

  return (j);
}

/*Go through all the intervals (as noted within the symbolTable), and split each one (if possible) */
/*Return the number of additional intervals*/
static size_t medianSplitEachInterval(uint32_t *symbolTable, size_t symbolCount) {
  size_t currentLeftEndpoint;
  size_t currentRightEndpoint;
  size_t addedBuckets;
  size_t j;

  /*Now, for each required halving, median split each existing interval*/
  currentLeftEndpoint = SIZE_MAX;
  currentRightEndpoint = SIZE_MAX;
  addedBuckets = 0;

  for (j = 0; j < symbolCount; j++) {
    if ((symbolTable[j * 3 + 2] & LEFTENDPOINTFLAG) != 0) {
      assert(currentLeftEndpoint == SIZE_MAX);
      currentLeftEndpoint = j;
    }

    assert(currentLeftEndpoint != SIZE_MAX);

    if ((symbolTable[j * 3 + 2] & RIGHTENDPOINTFLAG) != 0) {
      assert(currentRightEndpoint == SIZE_MAX);
      currentRightEndpoint = j;
      // Split the interval
      addedBuckets += medianSplit(symbolTable, currentLeftEndpoint, currentRightEndpoint);
      currentRightEndpoint = SIZE_MAX;
      currentLeftEndpoint = SIZE_MAX;
    }
  }
  return (addedBuckets);
}

static double bucketByMedianSplitting(uint32_t *symbolTable, size_t symbolCount, /*@out@*/ uint32_t *intervals, size_t *buckets, double targetPopulation) {
  size_t expBuckets;
  size_t logBuckets;
  size_t currentBuckets;
  size_t lastBuckets;
  size_t targetBuckets;
  size_t i, j;
  double curChi, oldChi;
  uint32_t *lastIntervals;

  /*Find the nearest power of 2 equal to or greater than buckets (expBuckets)*/
  /*Put the log of expBuckets into logBuckets*/
  if (buckets == 0) {
    expBuckets = 1;
    logBuckets = 0;
  } else {
    expBuckets = *buckets - 1;
    expBuckets |= expBuckets >> 1;
    expBuckets |= expBuckets >> 2;
    expBuckets |= expBuckets >> 4;
    expBuckets |= expBuckets >> 8;
    expBuckets |= expBuckets >> 16;
    logBuckets = (size_t)(__builtin_popcountl(expBuckets));
    expBuckets++;
  }

  assert(expBuckets > 0);

  /*Set up interval table*/
  if ((lastIntervals = calloc(3 * 2 * expBuckets, sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for previous interval table");
    exit(EX_OSERR);
  }

  targetBuckets = *buckets;

  fprintf(stderr, "BBMS: Targeting %zu buckets (%zu initial cuts)\n", targetBuckets, logBuckets);

  /*Clear out any endpoints marked*/
  for (i = 0; i < symbolCount; i++) {
    symbolTable[i * 3 + 2] = 0;
  }

  /*Mark initial interval endpoints*/
  symbolTable[2] = LEFTENDPOINTFLAG;
  symbolTable[(symbolCount - 1) * 3 + 2] |= RIGHTENDPOINTFLAG;
  currentBuckets = 1;

  /*do the initial set of median splitting*/
  for (i = 0; i < logBuckets; i++) {
    currentBuckets += medianSplitEachInterval(symbolTable, symbolCount);
  }

  fprintf(stderr, "BBMS: Initial cuts rendered %zu intervals\n", currentBuckets);

  /*Form up the intervals from the initial approach*/
  j = extractIntervals(symbolTable, symbolCount, intervals);
  assert(j == currentBuckets);
  curChi = chiSquareScore(intervals, currentBuckets, targetPopulation, targetBuckets);
  oldChi = curChi;
  lastBuckets = currentBuckets;

  for (i = 0; (i < 10) && (currentBuckets < targetBuckets) && (curChi <= oldChi); i++) {
    lastBuckets = currentBuckets;
    memcpy(lastIntervals, intervals, 3 * sizeof(uint32_t) * lastBuckets);
    oldChi = curChi;

    currentBuckets += medianSplitEachInterval(symbolTable, symbolCount);

    j = extractIntervals(symbolTable, symbolCount, intervals);
    assert(j == currentBuckets);
    curChi = chiSquareScore(intervals, currentBuckets, targetPopulation, targetBuckets);
  }

  /*Should we revert to the prior value?*/
  if (oldChi < curChi) {
    memcpy(intervals, lastIntervals, 3 * sizeof(uint32_t) * lastBuckets);
    currentBuckets = lastBuckets;
    curChi = oldChi;
  }

  assert(lastIntervals != NULL);
  free(lastIntervals);
  lastIntervals = NULL;

  *buckets = currentBuckets;

  return (curChi);
}

static bool testIntervals(uint32_t *intervals, size_t numOfIntervals, size_t datalen) {
  size_t runningTotal;
  size_t i;

  fprintf(stderr, "Testing resulting intervals...\n");
  runningTotal = 0;
  for (i = 0; i < numOfIntervals - 1; i++) {
    // Is the start point larger than the end point?
    // An interval should be start <= end.
    if (intervals[3 * i] > intervals[3 * i + 1]) {
      return (false);
    }

    // Make sure that the intervals are well ordered.
    if (intervalCompare(intervals + 3 * i, intervals + 3 * (i + 1)) >= 0) {
      return (false);
    }
    runningTotal += intervals[3 * i + 2];
  }
  runningTotal += intervals[3 * i + 2];

  // Make sure that we account for all of the data elements
  if (runningTotal != datalen) {
    return (false);
  }

  return (true);
}

static uint32_t *generateSymbolTable(uint32_t *data, size_t L, size_t *outNumOfSymbols) {
  uint32_t m;
  uint32_t bits;
  double doubleBits;
  size_t i;
  size_t numOfSymbols;
  uint32_t symbolRun;
  uint32_t *lookupTable;
  uint32_t *sortedData;
  uint32_t *symbolTable;
  uint32_t curSymbol;

  assert(L > 0);

  m = 0;
  for (i = 0; i < L; i++) {
    if (data[i] > m) {
      m = data[i];
    }
  }

  doubleBits = ceil(log2((double)m + 1.0));
  assert(doubleBits >= 0.0);
  bits = (uint32_t)doubleBits;
  fprintf(stderr, "Maximum symbol is %u (%u bit)\n", m, bits);

  if (((double)(3 * m) < (((double)L) + 1.39 * ((double)L) * log2((double)L))) && (log2((double)m + 1.0) < 28.0)) {
    fprintf(stderr, "Using a look-up table approach\n");
    /*A table based approach is likely better*/
    /*3m + L ops*/
    if ((lookupTable = calloc((m + 1), sizeof(uint32_t))) == NULL) {
      perror("Can't allocate array for look-up array");
      exit(EX_OSERR);
    }

    /*m ops*/
    for (i = 0; i <= m; i++) {
      lookupTable[i] = 0;
    }

    /*L ops*/
    for (i = 0; i < L; i++) {
      lookupTable[data[i]]++;
    }

    /*m ops*/
    numOfSymbols = 0;
    for (i = 0; i <= m; i++) {
      if (lookupTable[i] != 0) {
        numOfSymbols++;
      }
    }

    assert(numOfSymbols > 0);
    if ((symbolTable = calloc(numOfSymbols * 3, sizeof(uint32_t))) == NULL) {
      perror("Can't allocate array for symbol table");
      exit(EX_OSERR);
    }

    /*m ops*/
    curSymbol = 0;
    for (i = 0; i <= m; i++) {
      if (lookupTable[i] != 0) {
        symbolTable[3 * curSymbol] = (uint32_t)i;  // the raw data element
        symbolTable[3 * curSymbol + 1] = lookupTable[i];  // population of this symbol
        symbolTable[3 * curSymbol + 2] = 0;  // no flags just yet
        curSymbol++;
      }
    }

    assert(curSymbol == numOfSymbols);
    assert(lookupTable != NULL);
    free(lookupTable);
    lookupTable = NULL;

  } else {
    /*Likely better to do a sorting-based approach*/
    /*Or the lookup table is likely to be larger than 1 GB*/
    /*2L + 1.39 * L * log2(L) operations*/

    fprintf(stderr, "Using a sorting approach\n");

    if ((sortedData = calloc(L, sizeof(uint32_t))) == NULL) {
      perror("Can't allocate array for sorted data");
      exit(EX_OSERR);
    }

    fprintf(stderr, "Copying data...\n");

    /*L ops*/
    memcpy(sortedData, data, L * sizeof(uint32_t));

    fprintf(stderr, "Sorting data...\n");

    /*1.39 * L * log2(L) ops on average*/
    qsort(sortedData, L, sizeof(uint32_t), uintcompare);

    fprintf(stderr, "Tabulating symbols...\n");

    if ((symbolTable = calloc(L * 3, sizeof(uint32_t))) == NULL) {
      perror("Can't allocate array for symbol table");
      exit(EX_OSERR);
    }

    symbolRun = 1;  // the first element is always a novel symbol, and occurs at least once.
    numOfSymbols = 0;

    /*L ops*/
    for (i = 1; i < L; i++) {
      // Account for sortedData[i-1]
      if (sortedData[i] == sortedData[i - 1]) {
        symbolRun++;
      } else {
        symbolTable[3 * numOfSymbols] = sortedData[i - 1];  // the raw symbol
        symbolTable[3 * numOfSymbols + 1] = symbolRun;  // The population of symbols
        symbolTable[3 * numOfSymbols + 2] = 0;  // No flags yet
        symbolRun = 1;
        numOfSymbols++;
      }
    }

    // Account for sortedData[L-1]
    symbolTable[3 * numOfSymbols] = sortedData[L - 1];
    symbolTable[3 * numOfSymbols + 1] = symbolRun;
    symbolTable[3 * numOfSymbols + 2] = 0;
    numOfSymbols++;

    assert(sortedData != NULL);
    free(sortedData);
    sortedData = NULL;

    if ((symbolTable = realloc(symbolTable, numOfSymbols * 3 * sizeof(uint32_t))) == NULL) {
      perror("Cannot shrink to length of Symbol Table");
      exit(EX_OSERR);
    }
  }

  fprintf(stderr, "Found %zu symbols\n", numOfSymbols);
  *outNumOfSymbols = numOfSymbols;

  return (symbolTable);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t *data = NULL;
  uint32_t *symbolTable = NULL;
  uint32_t *BBGGintervals = NULL;
  uint32_t *BBOGintervals = NULL;
  uint32_t *BBMSintervals = NULL;
  uint32_t *rewriteTable = NULL;
  size_t BBGGoutputBuckets;
  size_t BBOGoutputBuckets;
  size_t BBMSoutputBuckets;
  double BBGGscore;
  double BBOGscore;
  double BBMSscore;
  size_t i;
  uint32_t outputBits;
  size_t outputBuckets;
  size_t datalen;
  uint32_t *curinterval;
  long outputInt;
  size_t numOfSymbols;
  double targetPopulation;
  uint32_t curLocation;
  size_t offset;
  statData_t outputSymbol;

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION(ULONG_MAX) > 32);
  assert(PRECISION(SIZE_MAX) > 32);

  if (argc != 3) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  outputInt = atoi(argv[2]);

  if ((outputInt <= 0) || (outputInt > STATDATA_BITS)) {
    useageExit();
  } else {
    outputBits = (uint32_t)outputInt;
  }

  outputBuckets = (size_t)(1UL << outputBits);
  fprintf(stderr, "Proceeding with %zu buckets\n", outputBuckets);

  datalen = readuint32file(infp, &data);
  if (datalen < 1) {
    useageExit();
  }

  assert(data != NULL);

  fprintf(stderr, "Read in %zu samples\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close input file");
    exit(EX_OSERR);
  }

  symbolTable = generateSymbolTable(data, datalen, &numOfSymbols);

  targetPopulation = ((double)datalen) / ((double)outputBuckets);
  fprintf(stderr, "Targeting %g samples per bucket\n", targetPopulation);

  fprintf(stderr, "Bucketing by grouping symbols...\n");
  assert(datalen > 0);
  if ((BBGGintervals = calloc(datalen * 3, sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for BBGG intervals table");
    exit(EX_OSERR);
  }

  assert(BBGGintervals != NULL);

  BBGGoutputBuckets = outputBuckets;
  BBGGscore = bucketByGreedyGrouping(symbolTable, numOfSymbols, BBGGintervals, &BBGGoutputBuckets, targetPopulation);
  fprintf(stderr, "Score: %.17g (%zu bins created.)\n", BBGGscore, BBGGoutputBuckets);

  assert(BBGGoutputBuckets > 0);
  if ((BBGGintervals = realloc(BBGGintervals, BBGGoutputBuckets * sizeof(uint32_t) * 3)) == NULL) {
    perror("Cannot shrink to length of BBGGintervals");
    exit(EX_OSERR);
  }

  assert(testIntervals(BBGGintervals, BBGGoutputBuckets, datalen));

  fprintf(stderr, "Bucketing by median splitting...\n");
  if ((BBMSintervals = calloc(datalen * 3, sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for BBMS intervals table");
    exit(EX_OSERR);
  }

  assert(BBMSintervals != NULL);

  BBMSoutputBuckets = outputBuckets;
  BBMSscore = bucketByMedianSplitting(symbolTable, numOfSymbols, BBMSintervals, &BBMSoutputBuckets, targetPopulation);
  fprintf(stderr, "Score: %.17g (%zu bins created.)\n", BBMSscore, BBMSoutputBuckets);
  assert(BBMSoutputBuckets > 0);

  if ((BBMSintervals = realloc(BBMSintervals, BBMSoutputBuckets * sizeof(uint32_t) * 3)) == NULL) {
    perror("Cannot shrink to length of BBMSintervals");
    exit(EX_OSERR);
  }

  assert(testIntervals(BBMSintervals, BBMSoutputBuckets, datalen));

  fprintf(stderr, "Bucketing by greedy binning...\n");
  if ((BBOGintervals = calloc(datalen * 3, sizeof(uint32_t))) == NULL) {
    perror("Can't allocate array for BBOG intervals table");
    exit(EX_OSERR);
  }

  BBOGoutputBuckets = outputBuckets;
  BBOGscore = bucketByOrderedGreedy(symbolTable, numOfSymbols, BBOGintervals, &BBOGoutputBuckets, targetPopulation);
  fprintf(stderr, "Score: %.17g (%zu bins created.)\n", BBOGscore, BBOGoutputBuckets);
  assert(BBOGoutputBuckets > 0);

  if ((BBOGintervals = realloc(BBOGintervals, BBOGoutputBuckets * sizeof(uint32_t) * 3)) == NULL) {
    perror("Cannot shrink to length of BBOGintervals");
    exit(EX_OSERR);
  }

  assert(testIntervals(BBOGintervals, BBOGoutputBuckets, datalen));

  if ((BBGGscore <= BBOGscore) && (BBGGscore <= BBMSscore)) {
    fprintf(stderr, "Bucketing by greedy global grouping was more successful...\n");
    /*Keep BBGG*/
    rewriteTable = BBGGintervals;
    BBGGintervals = NULL;
    outputBuckets = BBGGoutputBuckets;

    /*Free the others*/
    assert(BBOGintervals != NULL);
    free(BBOGintervals);
    BBOGintervals = NULL;
    assert(BBMSintervals != NULL);
    free(BBMSintervals);
    BBMSintervals = NULL;
  } else if ((BBOGscore <= BBGGscore) && (BBOGscore <= BBMSscore)) {
    fprintf(stderr, "Bucketing by ordered greedy grouping was more successful...\n");
    /*Keep BBOG*/
    rewriteTable = BBOGintervals;
    BBOGintervals = NULL;
    outputBuckets = BBOGoutputBuckets;

    /*Free the others*/
    assert(BBGGintervals != NULL);
    free(BBGGintervals);
    BBGGintervals = NULL;
    assert(BBMSintervals != NULL);
    free(BBMSintervals);
    BBMSintervals = NULL;
  } else {
    /*Keep BBMS*/
    fprintf(stderr, "Bucketing by median splitting was more successful...\n");
    rewriteTable = BBMSintervals;
    BBMSintervals = NULL;
    outputBuckets = BBMSoutputBuckets;

    /*Free the others*/
    assert(BBGGintervals != NULL);
    free(BBGGintervals);
    BBGGintervals = NULL;
    assert(BBOGintervals != NULL);
    free(BBOGintervals);
    BBOGintervals = NULL;
  }

  assert(symbolTable != NULL);
  free(symbolTable);
  symbolTable = NULL;
  assert(outputBuckets - 1 <= STATDATA_MAX);

  fprintf(stderr, "Translating data...\n");
  for (i = 0; i < datalen; i++) {
    if ((curinterval = bsearch(data + i, rewriteTable, outputBuckets, 3 * sizeof(uint32_t), intervalMembershipCompare)) == NULL) {
      perror("Can't find the correct interval.");
      exit(EX_DATAERR);
    }

    assert(curinterval >= rewriteTable);
    assert((curinterval - rewriteTable) % 3 == 0);
    offset = (uint64_t)(curinterval - rewriteTable) / 3;
    assert(offset <= UINT32_MAX);
    curLocation = (uint32_t)offset;

    assert(((size_t)curLocation) < outputBuckets);

    outputSymbol = (statData_t)curLocation;

    if (fwrite(&outputSymbol, sizeof(statData_t), 1, stdout) != 1) {
      perror("Can't write output to stdout");
      exit(EX_OSERR);
    }
  }

  free(rewriteTable);
  free(data);

  return EX_OK;
}
