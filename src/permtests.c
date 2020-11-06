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
#include <float.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include <bzlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "binio.h"
#include "precision.h"

#include "fancymath.h"
#include "globals-inst.h"
#include "randlib.h"
#include "translate.h"

#define NUMOFOFFSETS 5
#define PERMROUNDS 10000U
#define REPORTROUNDS 100U
#define TWOSIDEDERROR (PERMROUNDS / 2000)

#define EXCURSIONINDEX 0  // 5.1.1
#define NUMOFDIRRUNSINDEX 1  // 5.1.2
#define LONGESTDIRRUNINDEX 2  // 5.1.3
#define MAXCHANGESINDEX 3  // 5.1.4
#define NUMOFRUNSINDEX 4  // 5.1.5
#define LONGESTRUNINDEX 5  // 5.1.6
#define MEANCOLLISIONDISTINDEX 6  // 5.1.7
#define LONGESTCOLLISIONDISTINDEX 7  // 5.1.8
#define PERIODICITYINDEX 8  // 5.1.9
#define COVARIANCEINDEX PERIODICITYINDEX + NUMOFOFFSETS  // 5.1.10
#define COMPRESSIONINDEX COVARIANCEINDEX + NUMOFOFFSETS  // 5.1.11

// These are the results of the testing, mostly stored in the curData structure.
struct permResults {
  bool containsResults;
  double excursionResults;  // 5.1.1
  int64_t numOfDirRuns;  // 5.1.2
  int64_t longestDirRun;  // 5.1.3
  int64_t maxChanges;  // 5.1.4
  int64_t numOfRuns;  // 5.1.5
  int64_t longestRun;  // 5.1.6
  double meanCollisionDist;  // 5.1.7
  int64_t longestCollisionDist;  // 5.1.8
  int64_t periodicity[NUMOFOFFSETS];  // 5.1.9
  int64_t covariance[NUMOFOFFSETS];  // 5.1.10
  int64_t compressionResults;  // 5.1.11
};

// This is the per-thread-specific data, including working area for all the tests and the shuffled data.
struct testState {
  statData_t *shuffledData;
  statData_t *shuffledTranslatedData;
  char *workingData;  // For BZIP compression
  size_t workingDatalen;
  char *compressionString;  // The ascii representation of the values
  size_t index;
  bool *collisionTable;
};

struct curData {
  statData_t *data;
  statData_t *translatedData;
  size_t k;  // size of the alphabet
  size_t datalen;
  double mean;
  double translatedMedian;
  // Convention: claim passedMutex prior to claiming resultsMutex
  pthread_mutex_t resultsMutex;
  // C0 number of times permuted data is greater than reference
  unsigned int C0[9 + 2 * NUMOFOFFSETS];
  // C1 number of times permuted data is equal to reference
  unsigned int C1[9 + 2 * NUMOFOFFSETS];
  // C2 number of times permuted data is less than reference
  unsigned int C2[9 + 2 * NUMOFOFFSETS];
  pthread_mutex_t passedMutex;
  bool excursionTestingPassed;  // 5.1.1
  bool dirRunsTestingPassed;  // 5.1.2, 5.1.3, 5.1.4
  bool runsTestingPassed;  // 5.1.5, 5.1.6
  bool collisionTestingPassed;  // 5.1.7, 5.1.8
  bool periodicityTestingPassed;  // 5.1.9, 5.1.10
  bool compressionTestingPassed;  // 5.1.11
  struct permResults results[PERMROUNDS + 1];
  size_t finishedCycle;
};

static pthread_mutex_t nextToDomutex = PTHREAD_MUTEX_INITIALIZER;
static sem_t initialTestingFlag;

// The assignment sent to the threads
static uint32_t nextToDo = 0;
// Try to keep track of the types of the things that are passing
#define EXCURSIONTESTS 0x01
#define DIRRUNTESTS 0x02
#define RUNSTESTS 0x04
#define COLLISIONSTESTS 0x08
#define PERODICITYTESTS 0x10
#define COMPRESSIONTESTS 0x20
static uint32_t testsPassed = 0;
static uint32_t lastReportedPassed = 0;

static uint32_t configK = 2;
static bool configComplete = false;
static bool configDeterministic = false;

void *doTestingThread(void *ptr);

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "permtests [-v] [-t <n>] [-d] [-c] [-l <index>,<samples> ] <inputfile>\n");
  fprintf(stderr, "or\n");
  fprintf(stderr, "permtests [-v] [-b <p>] [-t <n>] [-k <m>] [-d] [-s <m>] [-c] -r\n");
  fprintf(stderr, "inputfile is assumed to be a sequence of " STATDATA_STRING " integers\n");
  fprintf(stderr, "-r \t instead of doing testing on provided data use a random iid variable.\n");
  fprintf(stderr, "-d \t Make any RNG input deterministic (also force one thread).\n");
  fprintf(stderr, "-b <p> \t Set the bias to <p>, that is the least likely symbol has probability k^(-p). Default is 1 (i.e., unbiased.)\n");
  fprintf(stderr, "-s <m> \t Use a sample set of <m> values. (default m=1000000)\n");
  fprintf(stderr, "-k <m> \t Use an alphabet of <m> values. (default k=2)\n");
  fprintf(stderr, "-v \t verbose mode. Repeating makes it more verbose.\n");
  fprintf(stderr, "-c \t Always complete all the tests.\n");
  fprintf(stderr, "-t <n> \t uses <n> computing threads. (default: number of cores * 1.3)\n");
  fprintf(stderr, "-l <index>,<samples>\tRead the <index> substring of length <samples>.\n");
  exit(EX_USAGE);
}

/*This is a variation on the Fisher Yates shuffle that leaves the original data un-permuted*/
/*Induces the same shuffle on indata and indata2, resulting in outdata and outdata2*/
/*This is the "inside-out" variation*/
/*See https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_%22inside-out%22_algorithm*/
static void FYInitShuffle(struct randstate *rstate, const statData_t *indata, const statData_t *indata2, size_t datalen, statData_t *outdata, statData_t *outdata2) {
  size_t i, j;

  assert(datalen > 0);
  assert(datalen <= UINT32_MAX);

  for (i = 0; i < datalen; i++) {
    j = randomRange((uint32_t)i, rstate);
    if (j != i) {
      outdata[i] = outdata[j];
      outdata2[i] = outdata2[j];
    }

    outdata[j] = indata[i];
    outdata2[j] = indata2[i];
  }
}

/*This is only applied to binary data*/
static statData_t conversionIIAccess(const statData_t *shuffledData, size_t datalen, size_t index) {
  const statData_t *dataStart;
  statData_t curout;
  size_t bitCount, bitsInEnd;

  assert(index * 8 < datalen);

  bitsInEnd = datalen - index * 8;
  if (bitsInEnd > 8) bitsInEnd = 8;

  curout = 0;
  for (dataStart = shuffledData + index * 8, bitCount = 0; bitCount < bitsInEnd; bitCount++) {
    curout = (statData_t)((curout << 1) | (dataStart[bitCount] & 0x01));
  }

  return ((statData_t)(curout << (8 - bitsInEnd)));
}

/*Note that conversion I is just the popcount of conversion II*/
/*This is only applied to binary data*/
static statData_t conversionIAccess(const statData_t *shuffledData, size_t datalen, size_t index) {
  return (statData_t)__builtin_popcount(conversionIIAccess(shuffledData, datalen, index));
}

// 5.1.1
// this must act on raw data
// there is no difference for the binary case
static void excursionTesting(struct curData *inData, struct testState *curState) {
  int64_t sum;
  statData_t *curp;
  statData_t *end;
  double curScaledMean;  // some integer scaling of the mean (i \bar{X})
  double curMax;
  double curd;

  // A pointer to the last data item
  end = curState->shuffledData + inData->datalen - 1;

  curMax = 0.0;
  curScaledMean = 0.0;
  sum = 0;
  // Note that the last symbol necessarily has an excursion of 0,
  // so we can skip that one.
  for (curp = curState->shuffledData; curp < end; curp++) {
    sum += *curp;
    curScaledMean += inData->mean;
    curd = fabs((double)sum - curScaledMean);
    if (curd > curMax) {
      curMax = curd;
    }
  }

  inData->results[curState->index].excursionResults = curMax;
  inData->results[curState->index].containsResults = true;
}

#define CEILDIV8(X) (((X) >> 3) + ((((X)&0x7U) == 0) ? 0 : 1))
// 5.1.2, 5.1.3, 5.1.4
// Binary data acts on conversion I (popcount of conversion II)
static void dirRunsTesting(struct curData *inData, struct testState *curState) {
  int64_t longestRun;
  int64_t runCount;
  int64_t incCount;
  int64_t runLength;
  int32_t curSymbol;
  statData_t curRawSymbol;
  statData_t nextRawSymbol;
  int32_t lastSymbol;
  size_t localDatalen;
  size_t j;

  if (inData->k == 2) {
    localDatalen = CEILDIV8(inData->datalen);
  } else {
    localDatalen = inData->datalen;
  }

  assert(localDatalen <= INT64_MAX);
  // the data[0] and data[1] need to be good so we can generate rewritten symbols (s')
  assert(inData->datalen >= 2);
  assert(curState->shuffledData != NULL);

  longestRun = 0;
  runCount = 0;
  incCount = 0;
  runLength = 0;

  lastSymbol = -1;

  if (inData->k == 2) {
    // Use the translated data (in case there are exactly two symbols, but they aren't 0 and 1)
    curRawSymbol = conversionIAccess(curState->shuffledTranslatedData, inData->datalen, 0);
  } else {
    curRawSymbol = curState->shuffledData[0];
  }

  for (j = 1; j < localDatalen; j++) {
    if (inData->k == 2) {
      // Use the translated data (in case there are exactly two symbols, but they aren't 0 and 1)
      nextRawSymbol = conversionIAccess(curState->shuffledTranslatedData, inData->datalen, j);
    } else {
      nextRawSymbol = curState->shuffledData[j];
    }

    if (curRawSymbol > nextRawSymbol) {
      curSymbol = 0;
    } else {
      curSymbol = 1;
    }

    incCount += curSymbol;

    if (lastSymbol == -1) {
      runCount = 1;
      runLength = 1;
    } else if (curSymbol == lastSymbol) {
      runLength++;
    } else {
      runCount++;
      if (runLength > longestRun) {
        longestRun = runLength;
      }
      runLength = 1;
    }
    lastSymbol = curSymbol;
    curRawSymbol = nextRawSymbol;
  }

  if (runLength > longestRun) {
    longestRun = runLength;
  }

  inData->results[curState->index].numOfDirRuns = runCount;
  inData->results[curState->index].longestDirRun = longestRun;
  inData->results[curState->index].maxChanges = (incCount >= ((int64_t)localDatalen - 1 - incCount)) ? incCount : ((int64_t)localDatalen - 1 - incCount);

  inData->results[curState->index].containsResults = true;
}

// 5.1.5, 5.1.6
// for k=2 case, the median is 0.5.
// Note that in any case, the median is invariant across shuffles.
// We only have median information for translated information
static void runsTesting(struct curData *inData, struct testState *curState) {
  int64_t longestRun;
  int64_t runCount;
  int64_t runLength;
  int32_t lastSymbol;
  double localMedian;
  size_t j;
  int32_t curSymbol;

  assert(inData->datalen <= INT64_MAX);

  longestRun = 0;
  runCount = 0;
  runLength = 0;

  if (inData->k == 2) {
    localMedian = 0.5;
  } else {
    localMedian = inData->translatedMedian;
  }

  lastSymbol = -1;
  for (j = 0; j < inData->datalen; j++) {
    if (curState->shuffledTranslatedData[j] < localMedian) {
      curSymbol = 0;
    } else {
      curSymbol = 1;
    }

    if (lastSymbol == -1) {
      runCount = 1;
      runLength = 1;
    } else if (curSymbol == lastSymbol) {
      runLength++;
    } else {
      runCount++;
      if (runLength > longestRun) {
        longestRun = runLength;
      }
      runLength = 1;
    }
    lastSymbol = curSymbol;
  }

  // Now fix up the data to account for the last run
  if (runLength > longestRun) {
    longestRun = runLength;
  }

  // Record the results
  inData->results[curState->index].numOfRuns = runCount;
  inData->results[curState->index].longestRun = longestRun;

  inData->results[curState->index].containsResults = true;
}

// 5.1.7, 5.1.8
// We must work on translated data
// The k=2 case is convII of translated data
static void collisionTesting(struct curData *inData, struct testState *curState) {
  int64_t sum;
  int64_t longestCollisionDist = 0;
  int64_t collisionCount = 0;
  size_t i;
  int64_t j;
  statData_t cur;
  size_t l;
  size_t tableSize;
  size_t localDatalen;

  assert(inData->datalen <= INT64_MAX);
  assert(inData->datalen > 0);
  assert(inData->k >= 2);

  if (inData->k == 2) {
    localDatalen = CEILDIV8(inData->datalen);
    // packed data is at most in [0, 255]
    tableSize = 256;
  } else {
    localDatalen = inData->datalen;
    tableSize = inData->k;
  }

  for (i = 0; i < tableSize; i++) {
    curState->collisionTable[i] = false;
  }

  j = 0;
  sum = 0;
  for (l = 0; l < localDatalen; l++) {
    if (inData->k == 2) {
      cur = conversionIIAccess(curState->shuffledTranslatedData, inData->datalen, l);
    } else {
      cur = curState->shuffledTranslatedData[l];
    }

    assert(cur < tableSize);
    if (curState->collisionTable[cur]) {
      // A collision has occurred
      for (i = 0; i < tableSize; i++) {
        curState->collisionTable[i] = false;
      }
      sum += j;
      collisionCount++;
      if (j > longestCollisionDist) {
        longestCollisionDist = j;
      }
      j = 0;
    } else {
      j++;
      curState->collisionTable[cur] = true;
    }
  }

  if (collisionCount != 0) {
    inData->results[curState->index].meanCollisionDist = (double)sum / (double)collisionCount;
  } else {
    inData->results[curState->index].meanCollisionDist = DBL_INFINITY;
  }

  inData->results[curState->index].longestCollisionDist = longestCollisionDist;
  inData->results[curState->index].containsResults = true;
}

// 5.1.9, 5.1.10
static void periodicityTesting(struct curData *inData, struct testState *curState) {
  uint32_t offsets[NUMOFOFFSETS] = {1, 2, 8, 16, 32};
  uint32_t j;
  uint32_t i;
  statData_t curSymbol, distantSymbol;
  uint64_t product;
  int64_t *periodicity;
  int64_t *covariance;
  size_t localDatalen;
  bool sumOverflow;

  assert(inData->datalen <= INT64_MAX);
  assert(inData != NULL);
  assert(curState != NULL);

  if (inData->k == 2) {
    localDatalen = CEILDIV8(inData->datalen);
  } else {
    localDatalen = inData->datalen;
  }

  periodicity = inData->results[curState->index].periodicity;
  covariance = inData->results[curState->index].covariance;
  assert((periodicity != NULL) && (covariance != NULL));

  for (j = 0; j < NUMOFOFFSETS; j++) {
    periodicity[j] = 0;
    covariance[j] = 0;
  }

  for (i = 0; i < localDatalen; i++) {
    if (inData->k == 2) {
      // Use the translated data (in case there are exactly two symbols, but they aren't 0 and 1)
      curSymbol = conversionIAccess(curState->shuffledTranslatedData, inData->datalen, i);
    } else {
      curSymbol = curState->shuffledData[i];
    }

    for (j = 0; (j < NUMOFOFFSETS) && ((i + offsets[j]) < localDatalen); j++) {
      if (inData->k == 2) {
        // Use the translated data (in case there are exactly two symbols, but they aren't 0 and 1)
        distantSymbol = conversionIAccess(curState->shuffledTranslatedData, inData->datalen, i + offsets[j]);
      } else {
        distantSymbol = curState->shuffledData[i + offsets[j]];
      }

      if (curSymbol == distantSymbol) {
        periodicity[j]++;
      }

      // This attempts to catch integer overflows.
      product = (uint64_t)curSymbol * (uint64_t)distantSymbol;  // No overflow risk

      if (product > INT64_MAX) {
        sumOverflow = true;
      } else {
#ifdef UADDL_OVERFLOW
        sumOverflow = __builtin_saddl_overflow(covariance[j], (int64_t)product, &(covariance[j]));
#else
        if ((int64_t)(INT64_MAX - product) >= covariance[j]) {
          sumOverflow = false;
          covariance[j] += (int64_t)product;  // We've now verified that this isn't causing an overflow
        } else {
          sumOverflow = true;
        }
#endif
      }
      assert(!sumOverflow);
    }
  }

  inData->results[curState->index].containsResults = true;
}

// The NIST reference string format is a bit odd...
// the buffer string must be suitable large to contain the resulting string (+ 1 byte)
static uint32_t dataToString(statData_t *data, size_t datalen, char *buffer) {
  char curInt[10];
  char *curIntPlace;
  char *copyPtr;
  statData_t curQ, curR;
  size_t j;
  char *curOut;

  assert(datalen > 0);

  curOut = buffer;

  for (j = 0; j < datalen; j++) {
    if (data[j] == 0) {
      *curOut = '0';
      curOut++;
    } else {
      curIntPlace = curInt + 9;
      curQ = data[j];
      while (curQ != 0) {
        curR = curQ % 10;
        curQ = curQ / 10;
        *curIntPlace = (char)('0' + curR);
        curIntPlace--;
      }
      for (copyPtr = curIntPlace + 1; copyPtr < curInt + 10; copyPtr++) {
        *curOut = *copyPtr;
        curOut++;
      }
    }

    *curOut = ' ';
    curOut++;
  }

  curOut--;
  if (configVerbose > 5) {
    *curOut = '\0';
    fprintf(stderr, "Compression test string: %s\n", buffer);
  }

  // The final ' ' should be disregarded
  assert(curOut - buffer <= UINT32_MAX);
  return (uint32_t)(curOut - buffer);
}

// 5.1.11
static void compressionTesting(struct curData *inData, struct testState *curState) {
  int res;
  unsigned int workingLen, bufferLen;

  assert(curState->workingDatalen <= UINT32_MAX);
  bufferLen = dataToString(curState->shuffledData, inData->datalen, curState->compressionString);
  workingLen = (uint32_t)curState->workingDatalen;

  res = BZ2_bzBuffToBuffCompress(curState->workingData, &workingLen, curState->compressionString, bufferLen, /*blockSize*/ 5, /*Verbosity*/ 0, /*workFactor*/ 30);
  assert(res == BZ_OK);

  inData->results[curState->index].compressionResults = workingLen;
  inData->results[curState->index].containsResults = true;
}

/*Returns a Boolean establishing if work should be continued*/
static bool doPermTesting(struct curData *inData, struct testState *curState) {
  bool localExcursionTestingPassed = false;  // 5.1.1
  bool localDirRunsTestingPassed = false;  // 5.1.2, 5.1.3, 5.1.4
  bool localRunsTestingPassed = false;  // 5.1.5, 5.1.6
  bool localCollisionTestingPassed = false;  // 5.1.7, 5.1.8
  bool localPeriodicityTestingPassed = false;  // 5.1.9, 5.1.10
  bool localCompressionTestingPassed = false;  // 5.1.11
  bool canShortCircuit = false;
  size_t j;

  if (curState->index != 0) {
    if (pthread_mutex_lock(&(inData->passedMutex)) != 0) {
      perror("Can't lock passedMutex");
      pthread_exit(NULL);
    }

    localExcursionTestingPassed = inData->excursionTestingPassed;  // 5.1.1
    localDirRunsTestingPassed = inData->dirRunsTestingPassed;  // 5.1.2, 5.1.3, 5.1.4
    localRunsTestingPassed = inData->runsTestingPassed;  // 5.1.5, 5.1.6
    localCollisionTestingPassed = inData->collisionTestingPassed;  // 5.1.7, 5.1.8
    localPeriodicityTestingPassed = inData->periodicityTestingPassed;  // 5.1.9, 5.1.10
    localCompressionTestingPassed = inData->compressionTestingPassed;  // 5.1.11

    if (pthread_mutex_unlock(&(inData->passedMutex)) != 0) {
      perror("Can't unlock passedMutex");
      pthread_exit(NULL);
    }
  }

  // Do the actual testing
  inData->results[curState->index].containsResults = false;

  if (configComplete || !localExcursionTestingPassed) {
    excursionTesting(inData, curState);  // 5.1.1
  }

  if (configComplete || !localDirRunsTestingPassed) {
    dirRunsTesting(inData, curState);  // 5.1.2, 5.1.3, 5.1.4
  }

  if (configComplete || !localRunsTestingPassed) {
    runsTesting(inData, curState);  // 5.1.5, 5.1.6
  }

  if (configComplete || !localCollisionTestingPassed) {
    collisionTesting(inData, curState);  // 5.1.7, 5.1.8
  }

  if (configComplete || !localPeriodicityTestingPassed) {
    periodicityTesting(inData, curState);  // 5.1.9, 5.1.10
  }

  if (configComplete || !localCompressionTestingPassed) {
    compressionTesting(inData, curState);  // 5.1.11
  }

  // Update the status data
  if (curState->index != 0) {
    if (pthread_mutex_lock(&(inData->passedMutex)) != 0) {
      perror("Can't lock passedMutex");
      pthread_exit(NULL);
    }

    if (pthread_mutex_lock(&(inData->resultsMutex)) != 0) {
      perror("Can't lock resultsMutex");
      pthread_exit(NULL);
    }

    // Update C0, C1, C2, Passed flags
    // 5.1.1

// Note, this is using a GCC/Clang extension; sadly, this precludes using -pedantic
#define CHECKPERMRESULT(RESVAR, RESINDEX)                                                                                                         \
  ({                                                                                                                                              \
    if (inData->results[curState->index].RESVAR > inData->results[0].RESVAR) {                                                                    \
      inData->C0[(RESINDEX)]++;                                                                                                                   \
    } else if (inData->results[curState->index].RESVAR == inData->results[0].RESVAR) {                                                            \
      inData->C1[(RESINDEX)]++;                                                                                                                   \
    } else {                                                                                                                                      \
      inData->C2[(RESINDEX)]++;                                                                                                                   \
    };                                                                                                                                            \
    (((inData->C0[(RESINDEX)] + inData->C1[(RESINDEX)]) > TWOSIDEDERROR) && ((inData->C1[(RESINDEX)] + inData->C2[(RESINDEX)]) > TWOSIDEDERROR)); \
  })

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (configComplete || !localExcursionTestingPassed) {
      localExcursionTestingPassed = CHECKPERMRESULT(excursionResults, EXCURSIONINDEX);
      inData->excursionTestingPassed |= localExcursionTestingPassed;
    }
#pragma GCC diagnostic pop

    // 5.1.2, 5.1.3, 5.1.4
    if (configComplete || !localDirRunsTestingPassed) {
      localDirRunsTestingPassed = CHECKPERMRESULT(numOfDirRuns, NUMOFDIRRUNSINDEX);
      localDirRunsTestingPassed = CHECKPERMRESULT(longestDirRun, LONGESTDIRRUNINDEX) && localDirRunsTestingPassed;
      localDirRunsTestingPassed = CHECKPERMRESULT(maxChanges, MAXCHANGESINDEX) && localDirRunsTestingPassed;
      inData->dirRunsTestingPassed |= localDirRunsTestingPassed;
    }

    // 5.1.5, 5.1.6
    if (configComplete || !localRunsTestingPassed) {
      localRunsTestingPassed = CHECKPERMRESULT(numOfRuns, NUMOFRUNSINDEX);
      localRunsTestingPassed = CHECKPERMRESULT(longestRun, LONGESTRUNINDEX) && localRunsTestingPassed;
      inData->runsTestingPassed |= localRunsTestingPassed;
    }

    // 5.1.7, 5.1.8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    if (configComplete || !localCollisionTestingPassed) {
      localCollisionTestingPassed = CHECKPERMRESULT(meanCollisionDist, MEANCOLLISIONDISTINDEX);
      localCollisionTestingPassed = CHECKPERMRESULT(longestCollisionDist, LONGESTCOLLISIONDISTINDEX) && localCollisionTestingPassed;
      inData->collisionTestingPassed |= localCollisionTestingPassed;
    }
#pragma GCC diagnostic pop

    // 5.1.9, 5.1.10
    if (configComplete || !localPeriodicityTestingPassed) {
      localPeriodicityTestingPassed = true;
      for (j = 0; j < NUMOFOFFSETS; j++) {
        localPeriodicityTestingPassed = CHECKPERMRESULT(periodicity[j], PERIODICITYINDEX + j) && localPeriodicityTestingPassed;
        localPeriodicityTestingPassed = CHECKPERMRESULT(covariance[j], COVARIANCEINDEX + j) && localPeriodicityTestingPassed;
      }
      inData->periodicityTestingPassed |= localPeriodicityTestingPassed;
    }

    // 5.1.11
    if (configComplete || !localCompressionTestingPassed) {
      localCompressionTestingPassed = CHECKPERMRESULT(compressionResults, COMPRESSIONINDEX);
      inData->compressionTestingPassed |= localCompressionTestingPassed;
    }

    canShortCircuit = localExcursionTestingPassed && localDirRunsTestingPassed && localRunsTestingPassed && localCollisionTestingPassed && localPeriodicityTestingPassed && localCompressionTestingPassed;

    if (canShortCircuit && (inData->finishedCycle == 0)) {
      inData->finishedCycle = curState->index;
    }

    if(inData->excursionTestingPassed) testsPassed |= EXCURSIONTESTS;
    if(inData->dirRunsTestingPassed) testsPassed |= DIRRUNTESTS;
    if(inData->runsTestingPassed) testsPassed |= RUNSTESTS;
    if(inData->collisionTestingPassed) testsPassed |= COLLISIONSTESTS;
    if(inData->periodicityTestingPassed) testsPassed |= PERODICITYTESTS;
    if(inData->compressionTestingPassed) testsPassed |= COMPRESSIONTESTS;


    if (pthread_mutex_unlock(&(inData->resultsMutex)) != 0) {
      perror("Can't unlock resultsMutex");
      pthread_exit(NULL);
    }

    if (pthread_mutex_unlock(&(inData->passedMutex)) != 0) {
      perror("Can't unlock passedMutex");
      pthread_exit(NULL);
    }
  }

  return (configComplete || !canShortCircuit);
}

static void printResults(FILE *outfp, struct permResults *results) {
  uint32_t i;
/*
  struct permResults {
    double excursionResults;  // 5.1.1
    int64_t numOfDirRuns;  // 5.1.2
    int64_t longestDirRun;  // 5.1.3
    int64_t maxChanges;  // 5.1.4
    int64_t numOfRuns;  // 5.1.5
    int64_t longestRun;  // 5.1.6
    double meanCollisionDist;  // 5.1.7
    int64_t longestCollisionDist;  // 5.1.8
    int64_t periodicity[NUMOFOFFSETS];  // 5.1.9
    int64_t covariance[NUMOFOFFSETS];  // 5.1.10
    int64_t compressionResults;  // 5.1.11
  };*/

  if (results->excursionResults >= 0) fprintf(outfp, "\t Max excursion: %.17g\n", results->excursionResults);
  if (results->numOfDirRuns >= 0) fprintf(outfp, "\t Number of directional runs: %" PRId64 "\n", results->numOfDirRuns);
  if (results->longestDirRun >= 0) fprintf(outfp, "\t Longest directional run: %" PRId64 "\n", results->longestDirRun);
  if (results->maxChanges >= 0) fprintf(outfp, "\t Maximum Changes: %" PRId64 "\n", results->maxChanges);
  if (results->numOfRuns >= 0) fprintf(outfp, "\t Number of runs: %" PRId64 "\n", results->numOfRuns);
  if (results->longestRun >= 0) fprintf(outfp, "\t Longest run: %" PRId64 "\n", results->longestRun);
  if (results->meanCollisionDist >= 0) fprintf(outfp, "\t Mean Collision Dist: %.17g\n", results->meanCollisionDist);
  if (results->longestCollisionDist >= 0) fprintf(outfp, "\t Max Collision Dist: %" PRId64 "\n", results->longestCollisionDist);
  if (results->periodicity[0] >= 0) {
    fprintf(outfp, "\t Periodicity: ");
    for (i = 0; i < NUMOFOFFSETS; i++) {
      fprintf(outfp, "%" PRId64 " ", (results->periodicity)[i]);
    }
    fprintf(outfp, "\n");
    fprintf(outfp, "\t Covariance: ");
    for (i = 0; i < NUMOFOFFSETS; i++) {
      fprintf(outfp, "%" PRId64 " ", (results->covariance)[i]);
    }
    fprintf(outfp, "\n");
  }
  if (results->compressionResults >= 0) fprintf(outfp, "\t Compressed to: %" PRId64 "\n", results->compressionResults);
}

static bool permResult(struct curData *inData, int index) {
  bool NISTresult;
  int64_t C0, C1, C2;

  C0 = inData->C0[index];
  C1 = inData->C1[index];
  C2 = inData->C2[index];

  // This is equivalent to the NIST test
  if ((C0 + C1 > 5) && (C1 + C2 > 5)) {
    NISTresult = true;
  } else {
    NISTresult = false;
  }

  printf("GTR (C0): %" PRId64 ", EQR (C1): %" PRId64 ", LTR (C2): %" PRId64 ", Total: %" PRId64 ", Percentile: %.5g, Verdict: %s\n", C0, C1, C2, C0 + C1 + C2, ((double)(C1 + C2)) / ((double)(C0 + C1 + C2)), NISTresult ? "Pass" : "Fail");

  return NISTresult;
}

static void permTestingResults(struct curData *inData) {
  uint32_t offsets[NUMOFOFFSETS] = {1, 2, 8, 16, 32};
  int i;

  if (configVerbose > 0) {
    printf("Data results:\n");
    if (inData->results[0].containsResults) printResults(stdout, inData->results);
  }

  if (configVerbose > 1) {
    printf("Shuffle results:\n");
    for (i = 1; i <= (int)PERMROUNDS; i++) {
      if (inData->results[i].containsResults) {
        printf("Shuffle %d of %u:\n", i, PERMROUNDS);
        printResults(stdout, inData->results + i);
      }
    }
  }

  printf("Excursion Test, ");
  permResult(inData, EXCURSIONINDEX);

  printf("Number of Directional Runs Test, ");
  permResult(inData, NUMOFDIRRUNSINDEX);

  printf("Longest Directional Run Test, ");
  permResult(inData, LONGESTDIRRUNINDEX);

  printf("Numbers of Increases Directional Run Test, ");
  permResult(inData, MAXCHANGESINDEX);

  printf("Number of Runs Test, ");
  permResult(inData, NUMOFRUNSINDEX);

  printf("Longest Runs Test, ");
  permResult(inData, LONGESTRUNINDEX);

  printf("Mean Collision Test, ");
  permResult(inData, MEANCOLLISIONDISTINDEX);

  printf("Maximum Collision Test, ");
  permResult(inData, LONGESTCOLLISIONDISTINDEX);

  for (i = 0; i < NUMOFOFFSETS; i++) {
    printf("Periodicity Test, p=%d, ", offsets[i]);
    permResult(inData, PERIODICITYINDEX + i);
  }

  for (i = 0; i < NUMOFOFFSETS; i++) {
    printf("Covariance Test, p=%d, ", offsets[i]);
    permResult(inData, COVARIANCEINDEX + i);
  }

  printf("Compression results, ");
  permResult(inData, COMPRESSIONINDEX);

  if (inData->finishedCycle > 0) {
    printf("Testing could have concluded after %zu rounds\n", inData->finishedCycle);
  }
}

static uint32_t getassignment(void) {
  uint32_t assignment;

  if (pthread_mutex_lock(&nextToDomutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  assignment = nextToDo;

  if (nextToDo <= PERMROUNDS) {
    nextToDo++;

    // do the print within the mutex protected area to order the outputs
    if ((configVerbose > 1) || ((configVerbose == 1) && (((assignment % REPORTROUNDS)==0) || (testsPassed != lastReportedPassed)))) {
      lastReportedPassed = testsPassed;
      fprintf(stderr, "%jd Assigned Round: %u / %u.", (intmax_t)time(NULL), assignment, PERMROUNDS);
      if(testsPassed == 0x00) {
        fprintf(stderr, " No tests passed.\n");
      } else {
        fprintf(stderr, " Finished tests: ");
        if(testsPassed & EXCURSIONTESTS) fprintf(stderr, "Excursion ");
        if(testsPassed & DIRRUNTESTS) fprintf(stderr, "DirectedRuns ");
        if(testsPassed & RUNSTESTS) fprintf(stderr, "Runs ");
        if(testsPassed & COLLISIONSTESTS) fprintf(stderr, "Collision ");
        if(testsPassed & PERODICITYTESTS) fprintf(stderr, "Perodicity ");
        if(testsPassed & COMPRESSIONTESTS) fprintf(stderr, "Compression ");
        fprintf(stderr, "\n");
      }
    }
  }

  if (pthread_mutex_unlock(&nextToDomutex) != 0) {
    perror("Can't get mutex");
    pthread_exit(NULL);
  }

  return assignment;
}

void *doTestingThread(void *ptr) {
  struct randstate rstate;
  struct testState curState = {.shuffledData = NULL, .shuffledTranslatedData = NULL, .workingData = NULL, .workingDatalen = 0, .compressionString = NULL, .index = 0, .collisionTable = NULL};
  struct curData *inData;
  bool continueWork;
  size_t compressionStringLen;

  initGenerator(&rstate);

  // This is the per-thread-specific data, including working area for all the tests and the shuffled data.

  rstate.deterministic = configDeterministic;

  inData = (struct curData *)ptr;

  if ((curState.shuffledData = malloc(inData->datalen * sizeof(statData_t))) == NULL) {
    perror("Can't allocate memory for shuffled data");
    pthread_exit(NULL);
  }

  if ((curState.shuffledTranslatedData = malloc(inData->datalen * sizeof(statData_t))) == NULL) {
    perror("Can't allocate memory for shuffled translated data");
    free(curState.shuffledData);
    pthread_exit(NULL);
  }

  compressionStringLen = 11 * inData->datalen;
  curState.workingDatalen = ((uint32_t)(((double)compressionStringLen) * 1.01) + 600);
  if ((curState.workingData = malloc(sizeof(char) * curState.workingDatalen)) == NULL) {
    perror("Can't allocate memory for working data");
    free(curState.shuffledData);
    free(curState.shuffledTranslatedData);
    pthread_exit(NULL);
  }

  if ((curState.compressionString = malloc(sizeof(char) * compressionStringLen)) == NULL) {
    perror("Can't allocate memory for character buffer");
    free(curState.shuffledData);
    free(curState.shuffledTranslatedData);
    free(curState.workingData);
    pthread_exit(NULL);
  }

  if ((curState.collisionTable = malloc(sizeof(bool) * ((inData->k == 2) ? 256 : inData->k))) == NULL) {
    perror("Can't allocate memory for collision Table");
    free(curState.shuffledData);
    free(curState.shuffledTranslatedData);
    free(curState.workingData);
    free(curState.compressionString);
    pthread_exit(NULL);
  }

  seedGenerator(&rstate);

  curState.index = getassignment();

  continueWork = true;
  while (continueWork && (curState.index <= (uint32_t)PERMROUNDS)) {
    if (curState.index == 0) {
      // For the first data (reference data)
      if (configVerbose > 1) {
        fprintf(stderr, "Initial data\n");
      }
      memcpy(curState.shuffledData, inData->data, inData->datalen * sizeof(statData_t));
      memcpy(curState.shuffledTranslatedData, inData->translatedData, inData->datalen * sizeof(statData_t));
      continueWork = doPermTesting(inData, &curState);
      if (sem_post(&initialTestingFlag) < 0) {
        perror("Can't post to semaphore");
        pthread_exit(NULL);
      }
    } else {
      // All future assignments
      FYInitShuffle(&rstate, inData->data, inData->translatedData, inData->datalen, curState.shuffledData, curState.shuffledTranslatedData);
      continueWork = doPermTesting(inData, &curState);
    }

    if (continueWork) curState.index = getassignment();
  }

  if (configVerbose > 1) {
    fprintf(stderr, "Thread done.\n");
  }

  free(curState.shuffledData);
  free(curState.shuffledTranslatedData);
  free(curState.workingData);
  free(curState.compressionString);
  free(curState.collisionTable);

  pthread_exit(NULL);
}

static void initPermArray(struct permResults *results) {
  size_t j, k;

  for (j = 0; j <= PERMROUNDS; j++) {
    results[j].containsResults = false;
    results[j].excursionResults = -1.0;
    results[j].numOfDirRuns = -1;
    results[j].longestDirRun = -1;
    results[j].maxChanges = -1;
    results[j].numOfRuns = -1;
    results[j].longestRun = -1;
    results[j].meanCollisionDist = -1.0;
    results[j].longestCollisionDist = -1;
    for (k = 0; k < NUMOFOFFSETS; k++) {
      results[j].periodicity[k] = -1;
      results[j].covariance[k] = -1;
    }
    results[j].compressionResults = -1;
  }
}

int main(int argc, char *argv[]) {
  FILE *infp;
  uint32_t j;
  uint64_t sum;
  struct curData *inData;
  long cpuCount;
  uint32_t threadCount = 0;
  pthread_t *threads;
  long inparam;
  int opt;
  // uint32_t nloglls=1;
  bool useFile = true;
  uint32_t configRandDataSize = 1000000;
  struct randstate rstate;
  bool translated;
  size_t configSubsetIndex;
  size_t configSubsetSize;
  unsigned long long int inint;
  char *nextOption;

  configSubsetIndex = 0;
  configSubsetSize = 0;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  configDeterministic = false;
  configComplete = false;

  while ((opt = getopt(argc, argv, "vt:rs:b:k:dcl:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'c':
        configComplete = true;
        break;
      case 't':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 10000)) {
          useageExit();
        }
        threadCount = (uint32_t)inparam;
        break;
      case 'k':
        inparam = strtol(optarg, NULL, 0);
        if ((inparam <= 0) || (inparam > 0xFFFFFFFF)) {
          useageExit();
        }
        configK = (uint32_t)inparam;
        break;
      case 'r':
        useFile = false;
        break;
      case 'l':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          useageExit();
        }
        configSubsetIndex = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSubsetSize = inint;
        break;
      case 's':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 1000000000)) {
          useageExit();
        }
        configRandDataSize = (uint32_t)inparam;
        break;
      case 'd':
        rstate.deterministic = true;
        configDeterministic = true;
        threadCount = 1;
        break;
      default: /* '?' */
        fprintf(stderr, "Unexpected argument %c\n", opt);
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  seedGenerator(&rstate);

  if (threadCount == 0) {
    // Make a guess on the optimal number of threads based on active processors
    if ((cpuCount = sysconf(_SC_NPROCESSORS_ONLN)) <= 0) {
      perror("Can't get processor count");
      exit(EX_OSERR);
    }

    if (configVerbose > 0) {
      fprintf(stderr, "CPU Count: %ld\n", cpuCount);
    }

    threadCount = (uint32_t)floor(1.3 * (double)cpuCount);
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Using %u threads\n", threadCount);
  }

  if ((threads = malloc(sizeof(pthread_t) * (uint32_t)threadCount)) == NULL) {
    perror("Can't allocate memory for thread IDs");
    exit(EX_OSERR);
  }

  if ((inData = malloc(sizeof(struct curData))) == NULL) {
    perror("Can't allocate memory for intra-thread data");
    exit(EX_OSERR);
  }

  // Initialize inData
  inData->data = NULL;
  inData->translatedData = NULL;
  inData->k = 0;
  inData->datalen = 0;
  inData->mean = 0.0;
  inData->translatedMedian = 0.0;

  if (pthread_mutex_init(&(inData->resultsMutex), NULL) != 0) {
    perror("Can't init mutex");
    exit(EX_OSERR);
  }
  for (j = 0; j < 9 + 2 * NUMOFOFFSETS; j++) {
    inData->C0[j] = 0;
    inData->C1[j] = 0;
    inData->C2[j] = 0;
  }
  if (pthread_mutex_init(&(inData->passedMutex), NULL) != 0) {
    perror("Can't init mutex");
    exit(EX_OSERR);
  }

  inData->excursionTestingPassed = false;  // 5.1.1
  inData->dirRunsTestingPassed = false;  // 5.1.2, 5.1.3, 5.1.4
  inData->runsTestingPassed = false;  // 5.1.5, 5.1.6
  inData->collisionTestingPassed = false;  // 5.1.7, 5.1.8
  inData->periodicityTestingPassed = false;  // 5.1.9, 5.1.10
  inData->compressionTestingPassed = false;  // 5.1.11
  inData->finishedCycle = 0;
  initPermArray(inData->results);

  fprintf(stderr, "Getting data...\n");

  // Get the data
  if (useFile) {
    if (argc != 1) {
      fprintf(stderr, "Currently have argc %d\n", argc);
      useageExit();
    }
    if ((infp = fopen(argv[0], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_NOINPUT);
    }

    inData->datalen = readuintfileloc(infp, &(inData->data), configSubsetIndex, configSubsetSize);
    assert(inData->datalen >= 16);

    if (fclose(infp) != 0) {
      perror("Couldn't close input data file");
      exit(EX_OSERR);
    }

    assert(inData->data != NULL);
  } else {
    if ((inData->data = malloc(configRandDataSize * sizeof(statData_t))) == NULL) {
      perror("Can't allocate buffer for data");
      exit(EX_OSERR);
    }

    genRandInts(inData->data, configRandDataSize, configK - 1, &rstate);
    inData->k = configK;
    inData->datalen = configRandDataSize;
    assert(inData->data != NULL);
    assert(inData->datalen >= 16);
  }

  if ((inData->translatedData = malloc(inData->datalen * sizeof(statData_t))) == NULL) {
    perror("Can't allocate buffer for translated data");
    exit(EX_OSERR);
  }

  memcpy(inData->translatedData, inData->data, inData->datalen * sizeof(statData_t));

  if (!(translated = translate(inData->translatedData, inData->datalen, &(inData->k), &(inData->translatedMedian)))) {
    free(inData->translatedData);
    inData->translatedData = inData->data;
  }

  if (configVerbose > 0) fprintf(stderr, "Median of translated data: %.17g\n", inData->translatedMedian);

  if (configVerbose > 0) {
    fprintf(stderr, "Testing %zu samples with %zu symbols\n", inData->datalen, inData->k);
  }

  // The mean is invariant across all permutations of the data set
  sum = 0;
  for (j = 0; j < (inData->datalen); j++) {
    sum += (inData->data)[j];
  }
  inData->mean = (double)sum / (double)(inData->datalen);

  if (sem_init(&initialTestingFlag, 0, 0) < 0) {
    perror("Can't create a semaphore");
    exit(EX_OSERR);
  }

  // start up the initial thread (which will initially calculate the reference data an record it in permResultArray[0])
  if (pthread_create(&(threads[0]), NULL, doTestingThread, (void *)inData) != 0) {
    perror("Can't create a thread");
    exit(EX_OSERR);
  }

  // wait for reference data
  // Note, this behaves as a memory barrier, so all future threads are guaranteed to have coherent permResultArray[0]
  if (sem_wait(&initialTestingFlag) < 0) {
    perror("Can't wait on the semaphore");
    exit(EX_OSERR);
  }

  // All done with the semaphore
  if (sem_destroy(&initialTestingFlag) < 0) {
    perror("Can't destroy the semaphore");
    exit(EX_OSERR);
  }

  // Now run the rest of the threads
  for (j = 1; j < threadCount; j++) {
    // Start up threads here
    if (pthread_create(&(threads[j]), NULL, doTestingThread, (void *)inData) != 0) {
      perror("Can't create a thread");
      exit(EX_OSERR);
    }
  }

  for (j = 0; j < threadCount; j++) {
    if (pthread_join(threads[j], NULL) != 0) {
      perror("Can't join thread");
      exit(EX_OSERR);
    }
  }

  permTestingResults(inData);

  free(threads);
  free(inData->data);
  if (translated) free(inData->translatedData);
  free(inData);

  return EX_OK;
}
