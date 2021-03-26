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
  size_t MLScount;
  size_t subsetSize;
  size_t simulationRounds;
  bool configFixedSymbol;
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
  fprintf(stderr, "-i <rounds>\t Use <rounds> simulation rounds. (Default is 2000000).\n");
  fprintf(stderr, "-t <n> \t uses <n> computing threads. (default: number of cores * 1.3)\n");
  fprintf(stderr, "-j <n> \t Each restart sanity vector is <n> elements long. (default: min(1000,samples,restart))\n");
  fprintf(stderr, "-m <t>,<simsym>\t For simulation, Use <t> maximal size symbols (the residual probability is evenly distributed amoungst the remaining simsym-t symbols).\n");
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

static size_t simulateBoundRound(size_t k, double p, size_t MLScount, size_t subsetSize, size_t *counts, bool configFixedSymbol, struct randstate *rstate) {
  double MLSprob;
  double extradelta;
  statData_t curSymbol;
  size_t curMax = 0;
  size_t j;
  double r;
  int64_t intOut;

  assert(MLScount > 0);
  assert(k >= MLScount);
  assert(k - 1 <= STATDATA_MAX);
  assert(p > 0.0);

  if (MLScount == k) {
    MLSprob = 1.0;
    extradelta = 0.0;
  } else {
    MLSprob = ((double)MLScount) * p;
    extradelta = (1.0 - MLSprob) / ((double)(k - MLScount));
  }

  assert((MLSprob <= 1.0) && (MLSprob >= 0.0));

  for (j = 0; j < k; j++) counts[j] = 0;

  for (j = 0; j < subsetSize; j++) {
    r = randomUnit(rstate);
    if (r < MLSprob) {
      intOut = (int64_t)floor(r / p);
      assert(intOut >= 0);
      assert(intOut <= STATDATA_MAX);

      curSymbol = (statData_t)intOut;
    } else {
      intOut = ((int64_t)floor((r - MLSprob) / extradelta)) + (int64_t)MLScount;
      assert(intOut >= 0);
      assert(intOut <= STATDATA_MAX);

      curSymbol = (statData_t)intOut;
    }

    if (configVerbose > 3) {
      fprintf(stderr, "Restart Sanity Test: Simulation Current Symbol = %u\n", curSymbol);
    }

    assert(curSymbol < k);
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
  assert((localThreadData->p > 0.0) && (localThreadData->p <= 1.0));
  assert(localThreadData->MLScount <= localThreadData->k);
  assert(localThreadData->results != NULL);

  if ((counts = malloc(localThreadData->k * sizeof(size_t))) == NULL) {
    perror("Can't allocate simulation counts array");
    exit(EX_OSERR);
  }

  seedGenerator(&rstate);

  for (j = 0; j < localThreadData->simulationRounds; j++) {
    localThreadData->results[j] = simulateBoundRound(localThreadData->k, localThreadData->p, localThreadData->MLScount, localThreadData->subsetSize, counts, localThreadData->configFixedSymbol, &rstate);
  }

  if (configVerbose > 1) {
    fprintf(stderr, "Thread done.\n");
  }

  free(counts);

  pthread_exit(NULL);
}

/*Returns the tightest bound such that the fail rate is less than alpha*/
/*The value returned should be treated as a pass. Any value larger than the returned value is a failue*/
static size_t simulateBound(double alpha, size_t k, double p, size_t MLScount, size_t subsetSize, size_t simulationRounds, bool configFixedSymbol) {
  pthread_t *threads;
  struct threadData baseThreadData;
  struct threadData *threadDataArray;
  size_t returnIndex;
  size_t j;
  size_t result;
  double extradelta;
  size_t taskRemainder, taskQuotient;
  size_t curIndex;

  /*We need fewer than SIZE_MAX repeats to be guarenteed*/
  assert(subsetSize != SIZE_MAX);

  assert(configThreadCount > 0);

  if (MLScount == 0) {
    MLScount = (size_t)floor(1.0 / p);
  }

  if (k == 0) {
    if (((double)MLScount) * p < 1.0) {
      k = MLScount + 1;
    } else {
      k = MLScount;
    }
  }

  assert(k >= MLScount);

  if (configVerbose > 1) {
    if (MLScount == k) {
      extradelta = 0.0;
    } else {
      extradelta = (1.0 - (((double)MLScount) * p)) / ((double)(k - MLScount));
    }

    fprintf(stderr, "Restart Sanity Test: Simulation MLS Count = %zu\n", MLScount);
    fprintf(stderr, "Restart Sanity Test: Simulation Symbols = %zu\n", k);
    fprintf(stderr, "Restart Sanity Test: Simulation MLS probability = %.17g\n", p);
    fprintf(stderr, "Restart Sanity Test: Simulation other symbol probability = %.17g\n", extradelta);
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
  baseThreadData.MLScount = MLScount;
  baseThreadData.subsetSize = subsetSize;
  baseThreadData.configFixedSymbol = configFixedSymbol;

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
  size_t localMax, Xmax, rowMax, colMax, rowIndex, colIndex;
  struct randstate rstate;
  long inparam;

  size_t configSubsetIndex;
  size_t configSubsetSize;
  bool configUseFile;
  bool configFixedSymbol;

  statData_t maxSymbol;
  bool configSimulateCutoff = true;
  size_t configSimulationRounds;
  size_t configSimulationSymbols;
  size_t configMLSCount;
  size_t configXmaxCutoff;
  long cpuCount;

  bool passVerdict;
  statData_t *newData;
  size_t configRestarts;
  size_t configSamplesPerRestart;

  configSimulateCutoff = true;
  configFixedSymbol = false;

  configRestarts = 1000;
  configSamplesPerRestart = 1000;
  alpha = 1.0 - exp(log(0.99) / ((double)(configRestarts + configSamplesPerRestart)));

  configSimulationRounds = (size_t)ceil(10.0 / alpha);
  configMLSCount = 0;
  configSimulationSymbols = 0;
  configXmaxCutoff = 0;

  configSubsetIndex = 0;
  configSubsetSize = 0;
  configUseFile = true;
  k = 2;

  initGenerator(&rstate);

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  while ((opt = getopt(argc, argv, "nvl:k:rc:ui:m:t:j:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'n':
        configFixedSymbol = true;
        break;
      case 'k':
        inparam = strtol(optarg, NULL, 0);
        if ((inparam <= 0) || (inparam > 0xFFFFFFFF)) {
          useageExit();
        }
        k = (uint32_t)inparam;
        break;
      case 't':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 10000)) {
          useageExit();
        }
        configThreadCount = (uint32_t)inparam;
        break;
      case 'u':
        configSimulateCutoff = false;
        break;
      case 'c':
        configSimulateCutoff = false;
        inparam = strtol(optarg, NULL, 0);
        if ((inparam <= 0) || (inparam > 0xFFFFFFFF)) {
          useageExit();
        }
        configXmaxCutoff = (uint32_t)inparam;
        break;
      case 'i':
        inparam = strtol(optarg, NULL, 0);
        if ((inparam <= 0) || (inparam > 0xFFFFFFFF)) {
          useageExit();
        }
        configSimulationRounds = (uint32_t)inparam;
        break;
      case 'm':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          useageExit();
        }
        configMLSCount = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSimulationSymbols = inint;
        break;
      case 'r':
        configUseFile = false;
        break;
      case 'd':
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (nextOption == NULL) || (*nextOption != ',')) {
          useageExit();
        }
        configRestarts = inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSamplesPerRestart = inint;
        break;
      case 'l':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSubsetIndex = inint;
        break;
      case 'j':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configSubsetSize = inint;
        break;
      default: /* ? */
        fprintf(stderr, "Invalid option %c\n", opt);
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if ((configUseFile && (argc != 2)) || (!configUseFile && (argc != 1))) {
    useageExit();
  }

  H_I = strtod(argv[0], NULL);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  if ((H_I <= 0) || (H_I == HUGE_VAL) || (H_I == -HUGE_VAL)) {
    perror("Can't extract entropy value");
    exit(EX_DATAERR);
  }
#pragma GCC diagnostic pop

  p = pow(2.0, -H_I);
  if (configVerbose > 0) {
    fprintf(stderr, "Restart Sanity Test: p = %.17g\n", p);
  }

  if (configSubsetSize == 0) {
    configSubsetSize = sizeMin(sizeMin(1000, configRestarts), configSamplesPerRestart);
  } else {
    if (configSubsetSize > sizeMin(configRestarts, configSamplesPerRestart)) {
      useageExit();
    }
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

    configThreadCount = (size_t)floor(1.3 * (double)cpuCount);
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Using %zu threads\n", configThreadCount);
  }

  seedGenerator(&rstate);

  if (configUseFile) {
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
    if ((data = malloc(configRestarts * configSamplesPerRestart * sizeof(statData_t))) == NULL) {
      perror("Can't allocate buffer for data");
      exit(EX_OSERR);
    }

    assert(k - 1 < UINT32_MAX);
    genRandInts(data, configRestarts * configSamplesPerRestart, (uint32_t)(k - 1), &rstate);
    datalen = configRestarts * configSamplesPerRestart;
  }

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
    configXmaxCutoff = simulateBound(alpha, configSimulationSymbols, p, configMLSCount, configSubsetSize, configSimulationRounds, configFixedSymbol);
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
