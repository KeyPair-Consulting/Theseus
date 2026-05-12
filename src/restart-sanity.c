/* This file is part of the Theseus distribution.
 * Copyright 2020-2026 Joshua E. Hill <josh@keypair.us>
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
#include <getopt.h>  // for optarg, getopt, optind
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>  // for uint32_t, SIZE_MAX, etc
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "entlib.h"
#include "fancymath.h"
#include "globals-inst.h"
#include "incbeta.h"
#include "precision.h"
#include "randlib.h"
#include "translate.h"

#include <pthread.h>

#define EX_FAIL 1

struct threadData {
  double alpha;
  size_t k;
  double p;
  size_t subsetSize;
  size_t simulationRounds;
  bool configFixedSymbol;
  bool configQuickPRNG;
  size_t *results;
};

static inline size_t sizeMax(size_t a, size_t b) {
  return (a >= b) ? a : b;
}

static inline size_t sizeMin(size_t a, size_t b) {
  return (a <= b) ? a : b;
}

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "restart-sanity [-t <n>] [-v] [-n] [-l <index> ] [-d <samples>,<restarts>] [-c] [-i <rounds>] <H_I> <inputfile>\nor\n");
  fprintf(stderr, "restart-sanity [-t <n>] [-v] [-n] [-k <m>]  [-d <samples>,<restarts>] [-c] [-i <rounds>] -r <H_I>\n");
  fprintf(stderr, "restart-sanity [-t <n>] [-v] [-d <samples>,<restarts>] [-c] [-i <rounds>] -f <X_max>\n");
  fprintf(stderr, "inputfile is assumed to be a sequence of " STATDATA_STRING " integers\n");
  fprintf(stderr, "H_I is the assessed entropy.\n");
  fprintf(stderr, "output is sent to stdout\n");
  fprintf(stderr, "-v \t verbose mode.\n");
  fprintf(stderr, "-l <index>\tRead the <index> substring of length <samples * restarts>.\n");
  fprintf(stderr, "-d <samples>,<restarts>\tPerform the restart testing using the described matrix dimensions. (default is 1000x1000)\n");
  fprintf(stderr, "-r \t instead of doing testing on provided data use a random iid variable.\n");
  fprintf(stderr, "-k <m> \t Use an alphabet of <m> values. (default k=2)\n");
  fprintf(stderr, "-n \t Force counting a single fixed global symbol (Causes the test to be binomial).\n");
  fprintf(stderr, "-u \t Don't simulate the cutoff.\n");
  fprintf(stderr, "-c <Xmaxcutoff> \t Use the provided cutoff.\n");
  fprintf(stderr, "-i <rounds>\t Use <rounds> simulation rounds. (Default is 2097152 for 1000x1000 tests).\n");
  fprintf(stderr, "-I <rounds>\t Use <rounds> simulation rounds when searching and close. (Default is 64 times the base number of simulation rounds).\n");
  fprintf(stderr, "-t <n> \t uses <n> computing threads.\n");
  fprintf(stderr, "-t <p>%% \t uses <p>%% of the computing threads. (default: 130%%)\n");
  fprintf(stderr, "-j <n> \t Each restart sanity vector is <n> elements long. (default: min(1000,samples,restart))\n");
  fprintf(stderr, "-f\t Calculate a cutoff for the supplied X_max\n");
  fprintf(stderr, "-q\t Use a quick PRNG.\n");
  exit(EX_USAGE);
}

static void initResultsArray(size_t *results, size_t rounds) {
  size_t j;

  for (j = 0; j < rounds; j++) {
    results[j] = SIZE_MAX;
  }
}

static int sizetcompare(const void *in1, const void *in2) {
  const size_t *left;
  const size_t *right;

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

static size_t maxSelectedFixed(const statData_t *data, size_t step, size_t num, statData_t symbol) {
  size_t j;
  const statData_t *curp;
  size_t max;

  assert(data != NULL);

  max = 0;

  for (j = 0, curp = data; j < num; j++) {
    if (*curp == symbol) max++;
    curp += step;
  }

  return max;
}

static size_t maxSelected(const statData_t *data, size_t step, size_t num, size_t k, size_t *counts) {
  size_t j;
  const statData_t *curp;
  size_t max;

  assert(counts != NULL);
  assert(data != NULL);

  for (j = 0; j < k; j++) {
    counts[j] = 0;
  }

  for (j = 0, curp = data; j < num; j++) {
    counts[*curp]++;
    curp += step;
  }

  max = 0;
  for (j = 0; j < k; j++) {
    if (counts[j] > max) {
      max = counts[j];
    }
  }

  return max;
}

// Here, we simulate a "worst case" for the restart sanity test. This is "worst case" in the sense that the adopted distribution
// results in the largest acceptable collision bound for a given assessed entropy level, so if a data sample fails this
// test, it is likely to indicate an underlying problem.
//
// This "worst case" uses the "inverted near-uniform" family (see Hagerty-Draper "Entropy Bounds and Statistical Tests" for
// a full definition of this distribution and justification for its use here).
//
// This distribution has as many maximal probability symbols as possible (each occurring with probability p), and possibly one
// additional symbol that contains all the residual probability.
//
// If the probability for the most likely symbol is p, then there are floor(1/p) most likely symbols,
// each occurring with probability p and possibly one additional symbol that has all the remaining (1 - p floor(1/p)) chance.
// In this code, we generate a random unit value in the range [0, 1), and we need to map this to one of the ceil(1/p) possible
// output symbols.
//
// Note that the function x -> floor(x/p) yields
// [0p,1p) -> 0
// [1p, 2p) -> 1
// [2p, 3p) -> 2
// ...
// [(floor(1/p)-1)p, floor(1/p)p) -> floor(1/p)-1
// [ floor(1/p)p, 1 ) -> floor(1/p)
//
// As such, each of the first floor(1/p) symbols (0 through floor(1/p)-1) have probability p of occurring, and
// the symbol floor(1/p) has probability 1-floor(1/p)p of occurring, as desired.
//
// Note that if floor(1/p) = ceil(1/p) = 1/p, then there is no "residual" symbol, only 1/p most likely symbols.
//
// The array is 0-indexed, so we can use this map to establish the index directly.
static size_t simulateBoundRound(size_t k, double p, size_t subsetSize, size_t *counts, bool configFixedSymbol, struct randstate *rstate) {
  statData_t curSymbol;
  size_t curMax = 0;
  size_t j;

  memset(counts, 0, sizeof(size_t)*k);

  for (j = 0; j < subsetSize; j++) {
    // Note that (int)floor(randomUnit(rstate) / p) is the index map discussed in the above comments.
    curSymbol = (statData_t)floor(randomUnit(rstate)/p);

    if (configVerbose > 3) {
      fprintf(stderr, "Restart Sanity Test: Simulation Current Symbol = %u\n", curSymbol);
    }

    counts[curSymbol]++;
    if (configFixedSymbol) {
      if (curSymbol == 0) curMax++;
    } else {
      if (counts[curSymbol] > curMax) {
        curMax++;
      }
    }
  }

  if (configVerbose > 2) {
    fprintf(stderr, "Restart Sanity Test: Round Maximum Symbol Count = %zu\n", curMax);
  }

  return (curMax);
}

static void *simulateBoundThread(void *ptr) {
  size_t *counts;
  size_t j;
  struct randstate rstate;
  struct threadData *localThreadData;

  assert(ptr != NULL);

  initGenerator(&rstate);

  // This is the per-thread-specific data, including working area for all the tests and the shuffled data.
  localThreadData = (struct threadData *)ptr;

  assert((localThreadData->alpha > 0.0) && (localThreadData->alpha <= 1.0));
  assert(localThreadData->k > 1);
  assert(localThreadData->k - 1 <= STATDATA_MAX);
  assert((localThreadData->p > 0.0) && (localThreadData->p <= 1.0));
  assert((size_t)ceil(1/localThreadData->p) <= localThreadData->k);
  assert(localThreadData->results != NULL);

  if ((counts = malloc(localThreadData->k * sizeof(size_t))) == NULL) {
    perror("Can't allocate simulation counts array");
    exit(EX_OSERR);
  }

  seedGenerator(&rstate);
  if(localThreadData->configQuickPRNG) rstate.quick = true;

  for (j = 0; j < localThreadData->simulationRounds; j++) {
    localThreadData->results[j] = simulateBoundRound(localThreadData->k, localThreadData->p, localThreadData->subsetSize, counts, localThreadData->configFixedSymbol, &rstate);
  }

  if (configVerbose > 2) {
    fprintf(stderr, "Thread done.\n");
  }

  free(counts);

  pthread_exit(NULL);
}

/*Returns the tightest bound such that the fail rate is less than alpha*/
/*The value returned should be treated as a pass. Any value larger than the returned value is a failue*/
static size_t simulateBound(double alpha, size_t k, double H, size_t subsetSize, size_t simulationRounds, bool configFixedSymbol, bool configQuickPRNG) {
  pthread_t *threads;
  struct threadData baseThreadData;
  struct threadData *threadDataArray;
  size_t returnIndex;
  size_t j;
  size_t result;
  size_t taskRemainder, taskQuotient;
  size_t curIndex;
  double p;

  /*We need fewer than SIZE_MAX repeats to be guarenteed*/
  assert(subsetSize != SIZE_MAX);

  assert(configThreadCount > 0);
  assert(H > 0.0);
  assert(H <= 8.0);

  p = pow(2.0, -H);

  if (k == 0) {
    k = (size_t)ceil(1/p);
  }

  if (configVerbose > 1) {
    fprintf(stderr, "Restart Sanity Test: Simulation Symbols = %zu\n", k);
  }

  if ((threads = malloc(sizeof(pthread_t) * configThreadCount)) == NULL) {
    perror("Can't allocate memory for thread IDs");
    exit(EX_OSERR);
  }

  if ((baseThreadData.results = malloc(simulationRounds * sizeof(size_t))) == NULL) {
    perror("Can't allocate buffer for results");
    exit(EX_OSERR);
  }

  if ((threadDataArray = malloc(simulationRounds * sizeof(struct threadData))) == NULL) {
    perror("Can't allocate buffer for threadData");
    exit(EX_OSERR);
  }

  baseThreadData.alpha = alpha;
  baseThreadData.k = k;
  baseThreadData.p = p;
  baseThreadData.subsetSize = subsetSize;
  baseThreadData.configFixedSymbol = configFixedSymbol;
  baseThreadData.configQuickPRNG = configQuickPRNG;

  taskQuotient = simulationRounds / configThreadCount;
  taskRemainder = simulationRounds % configThreadCount;

  initResultsArray(baseThreadData.results, simulationRounds);

  // Now run the rest of the threads
  curIndex = 0;
  for (j = 0; j < configThreadCount; j++) {
    memcpy(threadDataArray + j, &baseThreadData, sizeof(struct threadData));
    threadDataArray[j].simulationRounds = taskQuotient + ((taskRemainder > j) ? 1 : 0);
    threadDataArray[j].results = baseThreadData.results + curIndex;
    curIndex += threadDataArray[j].simulationRounds;

    // Start up threads here
    if (pthread_create(&(threads[j]), NULL, simulateBoundThread, (void *)(threadDataArray + j)) != 0) {
      perror("Can't create a thread");
      exit(EX_OSERR);
    }
  }

  assert(curIndex == simulationRounds);

  for (j = 0; j < configThreadCount; j++) {
    if (pthread_join(threads[j], NULL) != 0) {
      perror("Can't join thread");
      exit(EX_OSERR);
    }
  }

  qsort(baseThreadData.results, simulationRounds, sizeof(size_t), sizetcompare);
  // SIZE_MAX is used as a flag
  assert(baseThreadData.results[simulationRounds - 1] != SIZE_MAX);

  returnIndex = ((size_t)floor((1.0 - alpha) * ((double)simulationRounds)));
  if (returnIndex >= 1) returnIndex--;
  assert(returnIndex < simulationRounds);

  if (configVerbose > 2) {
    fprintf(stderr, "Restart Sanity Test: Simulation Index = %zu\n", returnIndex);
    fprintf(stderr, "Restart Sanity Test: result[0]: %zu, XmaxCutoff: %zu\n", baseThreadData.results[0], baseThreadData.results[returnIndex]);
  }
  result = baseThreadData.results[returnIndex];

  free(threads);
  free(baseThreadData.results);
  free(threadDataArray);

  return (result);
}

//This should make 6 digits meaningful
#define SEEKEPSILON 0.0000001
// Technically, the number of rounds could be bigger, but in practice *very* small values shouldn't occur
// so this should be suggicient in almost all cases
#define MAXHIST 64 

static double maxEntropyForBound(double alpha, size_t k, size_t Xtarget, size_t subsetSize, size_t simulationRounds, size_t closeSimulationRounds, bool configFixedSymbol, bool configQuickPRNG, double *entBounds) {
  double Hcur; 
  size_t Xmax, Xmin, Xcur;
  size_t minCols;
  double Hmin[MAXHIST];
  size_t HminIndex;
  double Hmax[MAXHIST];
  size_t HmaxIndex;

  assert(alpha >= 0.0);
  assert(alpha <= 1.0);
  assert(subsetSize>0);
  assert(Xtarget <= subsetSize); // There can't be more than subsetSize repeats
  assert(simulationRounds > 0);

  if(k == 0)
    k=256;

  if(configVerbose > 0) fprintf(stderr, "Seeking a cutoff of %zu\n", Xtarget);
  if(configVerbose > 0) fprintf(stderr, "subsetSize %zu\n", subsetSize);
  if(configVerbose>0) fprintf(stderr, "Initially using %zu simulations per round.\n", simulationRounds);

  minCols = (subsetSize / k) + (((subsetSize%k)==0)?0:1); // With k symbols and subsetSize entries, at least one symbol must occur this number of times.
  if(configVerbose > 0) fprintf(stderr, "X_max has a minimum value of %zu\n", minCols);
  assert(Xtarget >= minCols);

  HminIndex = 0; 
  HmaxIndex = 0;

  if(entBounds != NULL) {
    Hmin[HminIndex] = entBounds[0];
    Hmax[HmaxIndex] = entBounds[1];

    if(configVerbose > 0) fprintf(stderr, "Calculating base Xmin: ");
    Xmin = simulateBound(alpha, k, Hmax[HmaxIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
    if(configVerbose > 0) fprintf(stderr, "Calculating base Xmax: ");
    Xmax = simulateBound(alpha, k, Hmin[HminIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
  } else {
    Hmin[HminIndex] = 0.0;
    Hmax[HmaxIndex] = log2((double)k);
    Xmax = subsetSize;
    if(configVerbose > 0) fprintf(stderr, "Calculating base Xmin: ");
    Xmin = simulateBound(alpha, k, Hmax[HmaxIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
  }

  if(configVerbose > 0) fprintf(stderr, "Starting ranges: Entropy in the interval ( %g, %g ] associated with the cutoff interval [ %zu, %zu )\n", Hmin[HminIndex], Hmax[HmaxIndex], Xmin, Xmax);

  while((Hmax[HmaxIndex] - Hmin[HminIndex]) > SEEKEPSILON) {
    bool refreshBounds = false;
    if(((Xmax - Xmin) == 1) && (simulationRounds < closeSimulationRounds)) {
      simulationRounds = closeSimulationRounds / 2; // This is immediately be multiplied by 2
      if(configVerbose>0) fprintf(stderr, "Increasing simulation rounds final refinement. ");
      refreshBounds = true;
    }

    if(!refreshBounds) {
      if(configVerbose > 0) fprintf(stderr, "History index (%zu, %zu): Entropy in the interval ( %g, %g ] associated with the cutoff interval [ %zu, %zu )\n", HminIndex, HmaxIndex, Hmin[HminIndex], Hmax[HmaxIndex], Xmin, Xmax);
      Hcur = (Hmax[HmaxIndex] + Hmin[HminIndex]) / 2.0; // Note underflow is a more likely problem than overflow.
      if(configVerbose > 0) fprintf(stderr, "Trying Hcur=%g\n", Hcur);
      assert(Hcur > Hmin[HminIndex]);
      assert(Hcur < Hmax[HmaxIndex]);

      Xcur = simulateBound(alpha, k, Hcur, subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
      if(configVerbose > 0) fprintf(stderr, "Xcur = %zu\n", Xcur);
    } else {
      Hcur = -1.0;
      Xcur = 0;
    }

    if(refreshBounds || (Xcur > Xmax) || (Xcur < Xmin)) {
      // This is a stochastic process, so it may be necessary to expand the simulation.
      // Note, in this case, we are not advancing, and may backtrack if necessary.
      simulationRounds *= 2;
      if(configVerbose>0) fprintf(stderr, "Trying again with %zu simulations.\n", simulationRounds);

      Xmin = simulateBound(alpha, k, Hmax[HmaxIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
      Xmax = simulateBound(alpha, k, Hmin[HminIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
      if(configVerbose > 0) {
        //We just regenerated the same interval
        fprintf(stderr, "Regenerated state: History index (%zu, %zu): Entropy in the interval ( %g, %g ] associated with the cutoff interval [ %zu, %zu )\n", HminIndex, HmaxIndex, Hmin[HminIndex], Hmax[HmaxIndex], Xmin, Xmax);
      }

      // Is backtracking required? If so, use bounds from the history.

      //Hmin backtracking
      while(Xtarget >= Xmax) {
        // Backtrack on Hmin/Xmax
        assert(HminIndex>0);
        HminIndex--;
        if(configVerbose>0) fprintf(stderr, "Backtracking Hmin to history level %zu.\n", HminIndex);

        Xmax = simulateBound(alpha, k, Hmin[HminIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
      }

      //Hmax backtracking
      while(Xtarget < Xmin) {
        // Backtrack on Hmax/Xmin
        assert(HmaxIndex>0);
        HmaxIndex--;
        if(configVerbose>0) fprintf(stderr, "Backtracking Hmax to history level %zu.\n", HmaxIndex);

        Xmin = simulateBound(alpha, k, Hmax[HmaxIndex], subsetSize, simulationRounds, configFixedSymbol, configQuickPRNG);
      }

      //We now have an interval that passes the assertions with the current simulation count
      //In this case, we did not reduce the search interval (indeed, we may have expanded it!), 
      //but these bounds will be used for the next round's Hcur / Xcur
    } else {
      //This is functioning as expected.

      if(Xcur > Xtarget) {
        //Xcur is larger than Xtarget, so our next guess should smaller.
        //So the next Hcur should be larger.
        //This is done by moving Hmin higher.
        HminIndex++;
        assert(HminIndex < MAXHIST);
        Hmin[HminIndex] = Hcur;
        Xmax = Xcur;
      } else {
        //Xcur is smaller than or equal to Xtarget, so our next guess should larger.
        //So the next Hcur should be smaller.
        //This is done by moving Hmax lower.
        HmaxIndex++;
        assert(HmaxIndex < MAXHIST);
        Hmax[HmaxIndex] = Hcur;
        Xmin = Xcur;
      }
    }
    assert((Xtarget < Xmax) && (Xtarget >= Xmin));
    assert(Hmax[HmaxIndex] > Hmin[HminIndex]);
  }

  if(configVerbose > 0) fprintf(stderr, "Found entropy in the interval ( %g, %g ] associated with the cutoff interval [ %zu, %zu )\n", Hmin[HminIndex], Hmax[HmaxIndex], Xmin, Xmax);
  return(Hmax[HmaxIndex]);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  int opt;
  statData_t *data = NULL;
  size_t datalen;
  unsigned long long int inint;
  char *nextOption;
  size_t j;
  size_t i;
  size_t k;
  double median;
  double p;
  double H_I;
  double pValue;
  double alpha;
  size_t *counts;
  size_t localMax, Xmax=0, rowMax, colMax, rowIndex, colIndex;
  struct randstate rstate;
  long unsigned int inuparam;

  size_t configSubsetIndex;
  size_t configSubsetSize;
  bool configUseFile;
  bool configQuickPRNG = false;
  bool configFixedSymbol;
  double configThreadMult = 130.0;

  statData_t maxSymbol;
  bool configSimulateCutoff = true;
  size_t configSimulationRounds=0;
  size_t configSimulationRoundsClose=0;
  size_t configSimulationRoundsMin;
  size_t configSimulationSymbols;
  size_t configXmaxCutoff;
  bool configReverseSearch = false;
  long cpuCount;

  bool passVerdict;
  statData_t *newData;
  size_t configRestarts;
  size_t configSamplesPerRestart;

  configSimulateCutoff = true;
  configFixedSymbol = false;

  configRestarts = 1000;
  configSamplesPerRestart = 1000;
  configSimulationSymbols = 0;
  configXmaxCutoff = 0;

  configSubsetIndex = 0;
  configSubsetSize = 0;
  configUseFile = true;
  k = 2;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  while ((opt = getopt(argc, argv, "nvl:k:rc:uI:i:m:t:j:qf:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'n':
        configFixedSymbol = true;
        break;
      case 'q':
        configQuickPRNG = true;
        break;
      case 'k':
        inuparam = strtoul(optarg, NULL, 0);
        if ((inuparam > 0xFFFFFFFF)) {
          fprintf(stderr, "Value is too large.\n");
          useageExit();
        }
        k = (uint32_t)inuparam;
        break;
      case 't':
        if(optarg != NULL) {
          size_t optlen = strlen(optarg);
          double indouble;
          if(optarg[optlen-1]=='%') {
            char *nextarg;
            indouble = strtod(optarg, &nextarg);
            if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) ) {
              fprintf(stderr, "Thread percentage is out of range.\n");
              useageExit();
            }
            configThreadMult = indouble;
            configThreadCount = 0;
          } else {
            inuparam = strtoul(optarg, NULL, 10);
            if (inuparam > 10000) {
              fprintf(stderr, "Value is too large.\n");
              useageExit();
            }
            configThreadMult = -1.0;
            configThreadCount = (size_t)inuparam;
          }
        } else {
          fprintf(stderr, "Thread specifier is required.\n");
          useageExit();
        }
        break;
      case 'u':
        configSimulateCutoff = false;
        break;
      case 'c':
        configSimulateCutoff = false;
        inuparam = strtoul(optarg, NULL, 0);
        if ((inuparam > 0xFFFFFFFF)) {
          fprintf(stderr, "Value is too large.\n");
          useageExit();
        }
        configXmaxCutoff = (uint32_t)inuparam;
        break;
      case 'i':
        inuparam = strtoul(optarg, NULL, 0);
        if ((inuparam > 0xFFFFFFFF)) {
          fprintf(stderr, "Value is too large.\n");
          useageExit();
        }
        configSimulationRounds = (uint32_t)inuparam;
        break;
      case 'I':
        inuparam = strtoul(optarg, NULL, 0);
        if ((inuparam > 0xFFFFFFFF)) {
          fprintf(stderr, "Value is too large.\n");
          useageExit();
        }
        configSimulationRoundsClose = (uint32_t)inuparam;
        break;
      case 'r':
        configUseFile = false;
        break;
      case 'd':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          fprintf(stderr, "Incorrect command line format.\n");
          useageExit();
        }
        configRestarts = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          fprintf(stderr, "Integer out of range.\n");
          useageExit();
        }
        configSamplesPerRestart = inint;
        break;
      case 'l':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          fprintf(stderr, "Integer out of range (2).\n");
          useageExit();
        }
        configSubsetIndex = inint;
        break;
      case 'j':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          fprintf(stderr, "Integer out of range (3).\n");
          useageExit();
        }
        configSubsetSize = inint;
        break;
      case 'f':
        configReverseSearch = true;
        inuparam = strtoul(optarg, NULL, 0);
        if ((inuparam > 0xFFFFFFFF)) {
          fprintf(stderr, "Value is too large.\n");
          useageExit();
        }
        Xmax = (uint32_t)inuparam;
        break;
      default: /* ? */
        fprintf(stderr, "Invalid option %c\n", opt);
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (configReverseSearch) {
    if(argc != 0) {
      fprintf(stderr, "Reverse search enabled and too many arguments.\n");
    }
  } else if ((configUseFile && (argc != 2)) || (!configUseFile && (argc != 1)) ) {
    fprintf(stderr, "Can't find mandatory argument.\n");
    useageExit();
  }

 alpha = 1.0 - exp(log(0.99) / ((double)(configRestarts + configSamplesPerRestart)));
 configSimulationRoundsMin = (size_t)ceil(10.0 / alpha);

  if(configSimulationRounds == 0) {
    configSimulationRounds = nextBinaryPower(configSimulationRoundsMin);
  } else if(configSimulationRounds < configSimulationRoundsMin) {
    fprintf(stderr, "Can't reduce the number of simulation rounds below the default number.\n");
    configSimulationRounds = nextBinaryPower(configSimulationRoundsMin);
  }

  if(configSimulationRoundsClose == 0) {
    configSimulationRoundsClose = configSimulationRounds * 64;
  } else if(configSimulationRoundsClose < configSimulationRounds) {
    fprintf(stderr, "Can't reduce the number of simulation rounds for close values below the baseline number of simulation rounds.\n");
    configSimulationRoundsClose = configSimulationRounds;
  }

  if (configThreadCount == 0) {
    // Make a guess on the optimal number of threads based on active processors
    if ((cpuCount = sysconf(_SC_NPROCESSORS_ONLN)) <= 0) {
      perror("Can't get processor count");
      exit(EX_OSERR);
    }

    if (configVerbose > 0) {
      fprintf(stderr, "CPU Count: %ld\n", cpuCount);
    }

    configThreadCount = (uint32_t)floor((configThreadMult / 100.0) * (double)cpuCount);
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Using %zu threads\n", configThreadCount);
  }

  if (configSubsetSize == 0) {
    configSubsetSize = sizeMin(sizeMin(1000, configRestarts), configSamplesPerRestart);
  } else {
    if (configSubsetSize > sizeMin(configRestarts, configSamplesPerRestart)) {
      fprintf(stderr, "configSubsetSize is too large as compared to min(configRestarts, configSamplesPerRestart)\n");
      useageExit();
    }
  }

  seedGenerator(&rstate);
  if(configQuickPRNG) rstate.quick = true;

  //Run the reverse search if requested.
  if(configReverseSearch) {
    double Hmax = maxEntropyForBound(alpha, configSimulationSymbols, Xmax, configSubsetSize, configSimulationRounds, configSimulationRoundsClose, configFixedSymbol, configQuickPRNG, NULL);
    printf("%.17g\n", Hmax);
    return EX_OK;
  } else {
   // We are using a supplied value for H_I; read it in.
   H_I = strtod(argv[0], NULL);
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wfloat-equal"
    if ((H_I <= 0) || (H_I == HUGE_VAL) || (H_I == -HUGE_VAL)) {
      perror("Can't extract entropy value");
      exit(EX_DATAERR);
    }
  #pragma GCC diagnostic pop

    //Get the corresponding value for the MLS probibility
    p = pow(2.0, -H_I);
    if (configVerbose > 0) {
      fprintf(stderr, "Restart Sanity Test: p = %.17g\n", p);
    }

    //We are either reading the data in, or generating it.
    if (configUseFile) {
      //Read the data in.
      if ((infp = fopen(argv[1], "rb")) == NULL) {
        perror("Can't open file");
        exit(EX_NOINPUT);
      }

      datalen = readuintfileloc(infp, &data, configSubsetIndex, configRestarts * configSamplesPerRestart);
      if (fclose(infp) != 0) {
        perror("Couldn't close input data file");
        exit(EX_OSERR);
      }

    } else {
      //Generate the data.
      if ((data = malloc(configRestarts * configSamplesPerRestart * sizeof(statData_t))) == NULL) {
        perror("Can't allocate buffer for data");
        exit(EX_OSERR);
      }

      assert(k - 1 < UINT32_MAX);
      genRandInts(data, configRestarts * configSamplesPerRestart, (uint32_t)(k - 1), &rstate);
      datalen = configRestarts * configSamplesPerRestart;
    }

    //We now have the data
    assert(data != NULL);
    assert(configRestarts * configSamplesPerRestart == datalen);

    if ((configRestarts != configSubsetSize) || (configSamplesPerRestart != configSubsetSize)) {
      // Take a subset of the data for testing (we can only deal with square matrices for testing)
      if ((newData = malloc(sizeof(statData_t) * configSubsetSize * configSubsetSize)) == NULL) {
        perror("Can't allocate buffer for data subset");
        exit(EX_OSERR);
      }

      for (i = 0; i < configSubsetSize; i++) {  // row
        for (j = 0; j < configSubsetSize; j++) {  // col
          newData[i * configSubsetSize + j] = data[i * configSamplesPerRestart + j];
        }
      }

      free(data);
      data = newData;
      datalen = configSubsetSize * configSubsetSize;
    }

    alpha = 1.0 - exp(log(0.99) / ((double)(2 * configSubsetSize)));

    if (configSimulateCutoff) {
      if (configVerbose > 0) {
        fprintf(stderr, "Restart Sanity Test: Simulation Rounds = %zu\n", configSimulationRounds);
      }
      configXmaxCutoff = simulateBound(alpha, configSimulationSymbols, H_I, configSubsetSize, configSimulationRounds, configFixedSymbol, configQuickPRNG);
      if ((configVerbose > 0)) {
        fprintf(stderr, "Restart Sanity Test: Simulated XmaxCutoff = %zu\n", configXmaxCutoff);
      }
    } else {
      if ((configVerbose > 0) && (configXmaxCutoff > 0)) {
        fprintf(stderr, "Restart Sanity Test: Provided XmaxCutoff = %zu\n", configXmaxCutoff);
      }
    }

    // Now translate the data
    translate(data, datalen, &k, &median);

    if ((counts = malloc(k * sizeof(size_t))) == NULL) {
      perror("Can't allocate memmory for counts matrix");
      exit(EX_OSERR);
    }

    if (configFixedSymbol) {
      // Establish the most likely symbol in this dataset
      for (j = 0; j < k; j++) {
        counts[j] = 0;
      }

      for (j = 0; j < datalen; j++) {
        counts[data[j]]++;
      }

      maxSymbol = 0;
      for (j = 1; j < k; j++) {
        if (counts[j] > counts[maxSymbol]) {
          maxSymbol = (statData_t)j;
        }
      }
    } else {
      maxSymbol = 0;
    }

    // Rows first
    rowMax = 0;
    rowIndex = 0;
    for (j = 0; j < configSubsetSize; j++) {  // Rows
      if (configFixedSymbol) {
        localMax = maxSelectedFixed(data + configSubsetSize * j, 1, configSubsetSize, maxSymbol);
      } else {
        localMax = maxSelected(data + configSubsetSize * j, 1, configSubsetSize, k, counts);
      }
      if( (localMax > configXmaxCutoff) && (configVerbose>0) ) {
        fprintf(stderr, "Row over the cutoff: X_{R%zu} = %zu\n", j+1, localMax);
      }
      if (localMax > rowMax) {
        rowMax = localMax;
        rowIndex = j;
      }
    }

    if (configVerbose > 0) {
      fprintf(stderr, "Restart Sanity Test: X_R = %zu", rowMax);
      if (configVerbose > 1) fprintf(stderr, " (row %zu)", rowIndex);
      fprintf(stderr, "\n");
    }
    // Columns next
    colMax = 0;
    colIndex = 0;
    for (j = 0; j < configSubsetSize; j++) {  // cols
      if (configFixedSymbol) {
        localMax = maxSelectedFixed(data + j, configSubsetSize, configSubsetSize, maxSymbol);
      } else {
        localMax = maxSelected(data + j, configSubsetSize, configSubsetSize, k, counts);
      }
      if( (localMax > configXmaxCutoff) && (configVerbose>0) ) {
        fprintf(stderr, "Column over the cutoff: X_{C%zu} = %zu\n", j+1, localMax);
      }
      if (localMax > colMax) {
        colMax = localMax;
        colIndex = j;
      }
    }
    if (configVerbose > 0) {
      fprintf(stderr, "Restart Sanity Test: X_C = %zu", colMax);
      if (configVerbose > 1) fprintf(stderr, " (column %zu)", colIndex);
      fprintf(stderr, "\n");
    }

    Xmax = sizeMax(rowMax, colMax);
    if (configVerbose > 0) {
      fprintf(stderr, "Restart Sanity Test: X_max = %zu\n", Xmax);
    }

    free(counts);
    free(data);

    if (configXmaxCutoff > 0) {
      if (Xmax > configXmaxCutoff) {
        passVerdict = false;
        printf("Restart Sanity Check Verdict: Fail\n");
      } else {
        passVerdict = true;
        printf("Restart Sanity Check Verdict: Pass\n");
      }
    } else {
      /*The cited function is P(X >= Xmax) = 1 - P(X < Xmax) = 1 - P(X <= Xmax - 1)
       *So, this is in terms of the CDF for the binomial distribution, which can be represented
       *in terms of the regularized Beta function. Indeed (due to symmetry of this function),
       *we calculate 1-I_{1-p} (1001-Xmax, Xmax). By symmetry, this is equal to I_p (Xmax, 1001 - Xmax)*/
      pValue = incbeta((double)Xmax, (double)(configSubsetSize + 1 - Xmax), p);

      if (!configFixedSymbol) {
        fprintf(stderr, "Using invalid binomial assumption. Results aren't meaningful.\n");
      }

      if (configVerbose > 0) {
        fprintf(stderr, "Restart Sanity Test: Cutoff Statistic = %.17g\n", pValue);
        fprintf(stderr, "Restart Sanity Test: Corrected pValue = %.17g\n", 1.0 - pow(1.0 - pValue, 2.0 * (double)configSubsetSize));
      }

      if (pValue < alpha) {
        passVerdict = false;
        printf("Restart Sanity Check Verdict: Fail\n");
      } else {
        passVerdict = true;
        printf("Restart Sanity Check Verdict: Pass\n");
      }
    }

    if (passVerdict) {
      return EX_OK;
    } else {
      return EX_FAIL;
    }
  }
}
