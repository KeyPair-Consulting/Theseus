/* This file is part of the Theseus distribution.
 * Copyright 2020-2024 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <signal.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sys/time.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <sys/file.h>


#include "assessments.h"
#include "bootstrap.h"
#include "binio.h"
#include "binutil.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"
#include "randlib.h"
#include "translate.h"

#define EX_ZERO 1

// For each possible dataset (raw, bitstring), we have options as to how to assess it.
// 1) A single assessment as per 90B (no bootstrapping required!)
// 2) A sequence of assessments, each on configEvaluationBlockSize samples, where the final estimator results are either
//   a) The test parameters are bootstrapped, producing a single more meaningful value for a single final test that in some sense applies to all the data.
//   b) The final per-block assessments are bootstrapped, producing a single more meaningful value. [Note: if confidence intervals are used within an estimator, the median should be used. If not the lower bound of a 99% confidence interval
//   should be used.]
//
//   Final assessment is the minimum of the performed assessments.

enum evaluationEnum { raw, bitstring, combined };

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] [-l <index>,<samples> ] inputfile\n");
  fprintf(stderr, "\tor\n");
  fprintf(stderr, "non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] -R <k>,<L> -f\n");
  fprintf(stderr, "inputfile is presumed to consist of " STATDATA_STRING " integers in machine format\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 10 times for increased verbosity).\n");
  fprintf(stderr, "-s\tSend verbose mode output to stdout.\n");
  fprintf(stderr, "-i\tCalculate H_bitstring and H_I.\n");
  fprintf(stderr, "-c\tConditioned output, only calculate H_bitstring.\n");
  fprintf(stderr, "-r\tRaw evaluation, do not calculate H_bitstring.\n");
  fprintf(stderr, "-l <index>,<samples>\tRead the <index> substring of length <samples>.\n");
  fprintf(stderr, "-b <bitmask>\tOnly tests whose bits are set in <bitmask> are performed.\n");
  fprintf(stderr, "-g\tUse little endian conventions for creation of the bitstring used in calculation of H_bitstring (extract data least-significant to most-significant bits).\n");
  fprintf(stderr, "-R <k>,<L>\tRandomly generate input data (L samples with k symbols) instead of reading a file.\n");
  fprintf(stderr, "-O <x>,<nu>\tGenerate RO bitwise data with normalized jitter percentage of <x>%% and normalized inter-sample expected phase change of <nu> (in interval [0,1)). Negative nu forces random generation per-block.\n");
  fprintf(stderr, "-f\tGenerate a random <nu> value only once, rather than on a per-evaluation-block basis.\n");
  fprintf(stderr, "-d\tMake all RNG operations deterministic.\n");
  fprintf(stderr, "-L <x>\tPerform evaluations on blocks of size x. Prevents a single assessment of the entire file (but this can be turned back on using the \"-S\" option).\n");
  fprintf(stderr, "-M\tReport the median result of block assessments and identify the block that is closest to this result.\n");
  fprintf(stderr, "-N <x>\tPerform <x> rounds of testing (only makes sense for randomly generated data).\n");
  fprintf(stderr, "-B <c>,<rounds>\tPerform bootstrapping for a <c>-confidence interval using <rounds>.\n");
  fprintf(stderr, "-P\tEstablish an overall assessment based on bootstrap of individual test parameters.\n");
  fprintf(stderr, "-F\tEstablish an overall assessment based on bootstrap of final assessments.\n");
  fprintf(stderr, "-S\tEstablish an overall assessment using a large block assessment.\n");
  fprintf(stderr, "-X <s>\tSerially XOR s consecutive random values.\n");
  fprintf(stderr, "The final assessment is the minimum of the overall assessments.\n");
  exit(EX_USAGE);
}

static void makeBitstring(statData_t *data, statData_t *bitData, size_t datalen, statData_t activeBits, bool configLittleEndian) {
  // Populate bitData
  statData_t bitsToDo;
  statData_t *curBitData;
  statData_t curBit;

  curBitData = bitData;
  for (size_t l = 0; l < datalen; l++) {
    if (configLittleEndian) {
      curBit = 0x01;
    } else {
      curBit = highBit(activeBits);
    }

    bitsToDo = activeBits;

    while (bitsToDo != 0) {
      if ((curBit & bitsToDo) != 0) {
        *curBitData = ((curBit & data[l]) == 0) ? 0 : 1;
        curBitData++;
        bitsToDo = (statData_t)(bitsToDo & (~curBit));
      }

      if (configLittleEndian) {
        curBit = (statData_t)(curBit << 1);
      } else {
        curBit = (statData_t)(curBit >> 1);
      }
    }
  }
  assert((size_t)(curBitData - bitData) == datalen * ((size_t)__builtin_popcount(activeBits)));
}

static double doAssessment(const statData_t *data, size_t datalen, size_t k, uint32_t configTestBitmask, struct entropyTestingResult *result, const char *label) {
  struct timespec startTime;
  struct timespec endTime;
  struct timespec overallStartTime;
  struct timespec overallEndTime;
  double minminent;
  double minIIDminent;
  double curminent, curminent2;
  size_t j;

  initEntropyTestingResult(label, result);

  minminent = DBL_INFINITY;
  minIIDminent = DBL_INFINITY;

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &overallStartTime);

  if (configTestBitmask & MCVESTIMATEMASK) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);

    curminent = mostCommonValueEstimate(data, datalen, k, &(result->mcv));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    minminent = curminent;
    minIIDminent = curminent;
    result->mcv.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((k == 2) && (configTestBitmask & COLSESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = collisionEstimate(data, datalen, &(result->cols));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if ((curminent >= 0) && (curminent < minminent)) {
      minminent = curminent;
    }
    result->cols.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((k == 2) && (configTestBitmask & MARKOVESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = markovEstimate(data, datalen, &(result->markov));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if (curminent < minminent) {
      minminent = curminent;
    }

    result->markov.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((k == 2) && (configTestBitmask & COMPESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = compressionEstimate(data, datalen, &(result->comp));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if ((curminent >= 0.0) && (curminent < minminent)) {
      minminent = curminent;
    }
    result->comp.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((configTestBitmask & SAESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    SAalgs(data, datalen, k, &(result->sa));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    curminent = result->sa.tTupleEntropy;
    curminent2 = result->sa.lrsEntropy;
    result->sa.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);

    if ((curminent >= 0) && (curminent < minminent)) {
      minminent = curminent;
    }
    if ((curminent2 >= 0.0) && (curminent2 < minminent)) {
      minminent = curminent2;
    }
  }

  if ((configTestBitmask & MCWESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = multiMCWPredictionEstimate(data, datalen, k, &(result->mcw));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if ((curminent >= 0.0) && (curminent < minminent)) {
      minminent = curminent;
    }
    result->mcw.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((configTestBitmask & LAGESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = lagPredictionEstimate(data, datalen, k, &(result->lag));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if (curminent < minminent) {
      minminent = curminent;
    }
    result->lag.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((configTestBitmask & TREEMMCESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = treeMultiMMCPredictionEstimate(data, datalen, k, &(result->mmc));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if (curminent < minminent) {
      minminent = curminent;
    }
    result->mmc.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  if ((configTestBitmask & TREELZ78YESTIMATEMASK)) {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
    curminent = treeLZ78YPredictionEstimate(data, datalen, k, &(result->lz78y));
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &endTime);
    if (curminent < minminent) {
      minminent = curminent;
    }
    result->lz78y.runTime = ((double)endTime.tv_sec + (double)endTime.tv_nsec * 1.0e-9) - ((double)startTime.tv_sec + (double)startTime.tv_nsec * 1.0e-9);
  }

  clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &overallEndTime);

  result->runTime = ((double)overallEndTime.tv_sec + (double)overallEndTime.tv_nsec * 1.0e-9) - ((double)overallStartTime.tv_sec + (double)overallStartTime.tv_nsec * 1.0e-9);

  if (configVerbose > 3) {
    for (j = 0; j < ERRORSLOTS; j++) {
      if (globalErrors[j] >= 0.0) fprintf(stderr, "%s rel errors = %.17g\n", errorLabels[j], globalErrors[j]);
    }
  }

  assert(isfinite(minminent) != 0);
  result->assessedEntropy = minminent;

  result->assessedIIDEntropy = minIIDminent;

  return minminent;
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  size_t bitDatalen = 0;
  statData_t *data = NULL;
  statData_t *bitData = NULL;
  size_t k = 0;
  int opt;
  unsigned long long int inint;
  size_t configSerialXOR;
  bool configBlockAssessmentMedian;
  size_t configSubsetIndex;
  size_t configSubsetSize;
  char *nextOption;
  uint32_t configTestBitmask;
  enum evaluationEnum configEval;
  size_t bitWidth;
  bool configLittleEndian;
  bool configUseFile;
  size_t configK;
  size_t configRandDataSize;
  size_t configEvaluationBlockSize;
  bool configLargeBlockAssessment;
  size_t blockCount;
  struct entropyTestingResult *rawResults;
  struct entropyTestingResult *binaryResults;
  bool configBootstrapAssessments;
  bool configFixedRandomNu;
  double indouble;
  size_t bitBlockSize;
  size_t startIndex;
  size_t evaluationBlockSize;
  struct randstate rstate;
  size_t configRandomRounds;
  statData_t activeBits = 0;
  double configRONu;
  bool configRingOscillator;
  double configJitterPercentage;
  double *blockResultsNonIID;
  double *blockResultsIID;
  double confidenceInterval[2];

  configVerbose = 0;
  configSubsetIndex = 0;
  configSubsetSize = 0;
  configTestBitmask = 0xFFFF;
  configEval = combined;
  configLittleEndian = false;
  configUseFile = true;
  configRandDataSize = 1000000;
  configK = 2;
  configEvaluationBlockSize = 0;
  configRandomRounds = 1;
  configRONu = 0.0;
  configRingOscillator = false;
  configJitterPercentage = 0.0;
  configFixedRandomNu = false;
  configSerialXOR = 1;

  // Assessment strategies
  configBootstrapParams = false;
  configLargeBlockAssessment = false;
  configBootstrapAssessments = false;
  configBlockAssessmentMedian = false;

  initGenerator(&rstate);

  while ((opt = getopt(argc, argv, "fvsicrl:b:gR:L:B:PFSN:O:dX:M")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 's':
        if (dup2(fileno(stdout), fileno(stderr)) != fileno(stderr)) {
          perror("Can't dup stdout to stderr");
          exit(EX_OSERR);
        }
        break;
      case 'i':
        configEval = combined;
        break;
      case 'c':
        configEval = bitstring;
        break;
      case 'r':
        configEval = raw;
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
      case 'b':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > UINT32_MAX)) {
          useageExit();
        }
        configTestBitmask = (uint32_t)inint;
        break;
      case 'f':
        configFixedRandomNu = true;
        break;
      case 'M':
        configBlockAssessmentMedian = true;
        break;
      case 'g':
        configLittleEndian = true;
        break;
      case 'R':
        configUseFile = false;
        inint = strtoull(optarg, &nextOption, 0);
        if ((inint < 2) || (inint > STATDATA_MAX + 1) || (errno == EINVAL)) {
          useageExit();
        }
        configK = (size_t)inint;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint < 1000) || (inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configRandDataSize = inint;
        break;
      case 'L':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > UINT32_MAX)) {
          useageExit();
        }
        configEvaluationBlockSize = (uint32_t)inint;
        break;
      case 'X':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > UINT32_MAX) || (inint < 2)) {
          useageExit();
        }
        configSerialXOR = (uint32_t)inint;
        break;
      case 'N':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > UINT32_MAX)) {
          useageExit();
        }
        configRandomRounds = (uint32_t)inint;
        break;
      case 'B':
        indouble = strtod(optarg, &nextOption);
        if ((*nextOption != ',') || (indouble > 1.0) || (indouble < 0.0)) {
          useageExit();
        }
        configBootstrapConfidence = indouble;
        nextOption++;
        inint = strtoull(nextOption, NULL, 0);
        if ((inint == 0) || (inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        configBootstrapRounds = inint;
        break;
      case 'O':
        indouble = strtod(optarg, &nextOption);
        if ((*nextOption != ',') || (indouble > 1000.0) || (indouble < 0.0)) {
          useageExit();
        }
        configRingOscillator = true;
        configJitterPercentage = indouble;
        nextOption++;

        indouble = strtod(nextOption, NULL);
        if ((indouble >= 1.0)) {
          useageExit();
        }
        configRONu = indouble;
        break;
      case 'P':
        configBootstrapParams = true;
        break;
      case 'F':
        configBootstrapAssessments = true;
        break;
      case 'S':
        configLargeBlockAssessment = true;
        break;
      case 'd':
        rstate.deterministic = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  seedGenerator(&rstate);

  if (configVerbose > 0) fprintf(stderr, "Verbosity set to %d\n", configVerbose);

  if (configUseFile) {
    // Taking data from a file
    if (argc != 1) {
      useageExit();
    }

    if (configRandomRounds != 1) {
      fprintf(stderr, "Using input files isn't compatible with multiple rounds of testing.\n");
      useageExit();
    }

    if(configVerbose > 0) {
      if(configSubsetSize == 0) printf("Opening file: '%s'\n", argv[0]);
      else printf("Opening file: '%s', reading block %zu of size %zu\n", argv[0], configSubsetIndex, configSubsetSize);
    }

    if ((infp = fopen(argv[0], "rb")) == NULL) {
      perror("Can't open file");
      exit(EX_NOINPUT);
    }

    datalen = readuintfileloc(infp, &data, configSubsetIndex, configSubsetSize);
    assert(data != NULL);

    if (fclose(infp) != 0) {
      perror("Couldn't close input data file");
      exit(EX_OSERR);
    }

    if(configSerialXOR > 1) {
      datalen=serialXOR(data, datalen, configSerialXOR);
      if(configVerbose > 0) fprintf(stderr, "Performing %zu:1 serial XOR compression on input data; new size is %zu symbols.\n", configSerialXOR, datalen);
    }

    if (configVerbose > 0) {
      fprintf(stderr, "Read in %zu integers\n", datalen);
    }

    if (datalen == 0) {
      fprintf(stderr, "No data found.\n");
      useageExit();
    }
  } else {
    // Using random data
    if (argc != 0) {
      useageExit();
    }

    if ((data = malloc(configRandDataSize * sizeof(statData_t))) == NULL) {
      perror("Can't allocate buffer for data");
      exit(EX_OSERR);
    }

    if(configSerialXOR > 1) {
      datalen = configRandDataSize / configSerialXOR;
    } else {
      datalen = configRandDataSize;
    }
  }

  // This assessment type doesn't support multiple rounds.
  if (configLargeBlockAssessment && (configRandomRounds > 1)) {
    fprintf(stderr, "Large Block Assessment isn't compatible with multiple rounds of testing.\n");
    useageExit();
  }

  if (configRingOscillator && configUseFile) {
    fprintf(stderr, "Must enable RNG use to use a RO.\n");
    useageExit();
  }

  // There are two ways for Nu to vary, if it is random.
  // If configRONu < 0, then this Nu value is generated randomly.
  // If configFixedRandomNu is true, then we want it to be a fixed randomly selected value (fixed for any number of generation blocks and rounds for a single invocation of this program).
  // If configFixedRandomNu is false, then Nu is reset for each generation Block.
  if (configFixedRandomNu) {
    if (!configRingOscillator) {
      fprintf(stderr, "Can only use random nu values when ring oscillator generation is selected.\n");
      useageExit();
    }
    if (configRONu >= 0.0) {
      fprintf(stderr, "Can only use random nu values when specified nu is negative.\n");
      useageExit();
    } else {
      configRONu = randomUnit(&rstate) / 4;
      if (configVerbose > 0) fprintf(stderr, "Setting fixed random value for Nu = %.17g\n", configRONu);
    }
  }

  if (configRingOscillator && (configK != 2)) {
    fprintf(stderr, "RO requires k=2.\n");
    useageExit();
  }

  assert(datalen > 0);

  if (configUseFile) {
    activeBits = getActiveBitsSD(data, datalen);
    bitWidth = (size_t)__builtin_popcount(activeBits);
  } else {
    bitWidth = (size_t)ceil(log2((double)configK));
    activeBits = (statData_t)((1U << bitWidth) - 1);
  }

  if (configEval != raw) {
    if (bitWidth > 1) {
      bitDatalen = datalen * bitWidth;

      // Allocate the bitstring
      if (configVerbose > 0) fprintf(stderr, "Symbol width: %zu. Total bits in bitstring: %zu.\n", bitWidth, bitDatalen);

      if ((bitData = malloc(sizeof(statData_t) * bitDatalen)) == NULL) {
        perror("Can't allocate array for bit data");
        if (data != NULL) {
          free(data);
          data = NULL;
        }
        exit(EX_OSERR);
      }
      if (configUseFile) makeBitstring(data, bitData, datalen, activeBits, configLittleEndian);
    } else {
      fprintf(stderr, "One bit symbols in use. Reverting to raw evaluation\n");
      configEval = raw;
      bitData = NULL;
      bitDatalen = 0;
    }
  }

  if (configEval != bitstring) {
    if (configUseFile) {
      double median;  // we are going to ignore this.

      translate(data, datalen, &k, &median);
      if (configVerbose > 0) {
        fprintf(stderr, "Found %zu symbols\n", k);
      }

      if (k < 2) {
        if (configVerbose > 0) {
          fprintf(stderr, "Sample cannot contain entropy!\n");
        }

        printf("Assessed min entropy = %.17g\n", 0.0);
        printf("Assessed min entropy (IID) = %.17g\n\n", 0.0);

        if (bitData != NULL) {
          free(bitData);
          bitData = NULL;
        }

        if (data != NULL) {
          free(data);
          data = NULL;
        }
        fflush(stdout);
        return EX_ZERO;
      }
    } else {
      k = configK;
    }
  }

  if (configEvaluationBlockSize > 0) {
    // We are a block size, but there may well be several of these split across several random rounds.
    if (datalen > configEvaluationBlockSize) {
      evaluationBlockSize = configEvaluationBlockSize;
      blockCount = datalen / evaluationBlockSize;
    } else {
      // In this instance, there is one partial block and no LBA.
      fprintf(stderr, "Not enough data for a single block. Performing the test on the partial block.\n");
      blockCount = 1;
      evaluationBlockSize = datalen;
      configLargeBlockAssessment = false;
    }

  } else {
    // datalen > 0
    blockCount = 1;
    evaluationBlockSize = datalen;
  }
  bitBlockSize = evaluationBlockSize * bitWidth;

  // Print message summarizing the calculation.
  if (configVerbose > 0) {
    fprintf(stderr, "Performing ");
    if (configRandomRounds > 1) fprintf(stderr, "%zu rounds of ", configRandomRounds);
    if (blockCount > 1) {
      fprintf(stderr, "%zu assessments each ", blockCount);
    } else {
      fprintf(stderr, "an assessment ");
    }
    fprintf(stderr, "of size %zu symbols\n", evaluationBlockSize);
  }

  // Now perform some sanity checks to see if we should actually run the multi-block tests.
  if ((configRandomRounds * blockCount <= 1) && (configBootstrapParams || configBootstrapAssessments || configBlockAssessmentMedian)) {
    fprintf(stderr, "Multi-block assessment strategies are only compatible with multiple blocks of testing. Reverting to a single round of testing.\n");
    configBootstrapParams = false;
    configBootstrapAssessments = false;
    configBlockAssessmentMedian = false;
  }

  //Similarly, the LBA only makes sense if we have more than one block of data.
  if (configLargeBlockAssessment && (evaluationBlockSize >= datalen)) {
    fprintf(stderr, "Large Block Assessment is only valid for multi-block testing.\n");
    configLargeBlockAssessment = false;
  }

  if(configLargeBlockAssessment) {
    startIndex = 0;
  } else {
    startIndex = 1;
  }

  if ((datalen <= evaluationBlockSize) && configLargeBlockAssessment) {
    fprintf(stderr, "Large block assessment is only compatible with data sizes larger than a single block. Disabling LBA testing.\n");
    configLargeBlockAssessment = false;
  }

  // "At least 139 data elements" is to make it so that we expect at least 1 sample to be more extreme than the 0.5% percentile.
  // This is calculated using the binomial CDF; in particular, note that F(0; 138, 0.005) = 0.500709 and
  // F(0; 139, 0.005) = 0.498205, so we expect at least 1 element to be more extreme so long as we have at least 139 samples.
  if (configBootstrapParams && (configRandomRounds * blockCount < 139)) {
    fprintf(stderr, "Can't bootstrap parameters with less than 139 blocks. Skipping parameter bootstrap.\n");
    configBootstrapParams = false;
  }

  if (configEval != bitstring) {
    if ((rawResults = calloc(configRandomRounds * blockCount + 1, sizeof(struct entropyTestingResult))) == NULL) {
      perror("Can't allocate buffer for raw results");
      exit(EX_OSERR);
    }
  } else {
    rawResults = NULL;
  }

  if (configEval != raw) {
    // Note that, in non-raw modes, non-binary data is still evaluated as binary data (to calculate H_bitstring).
    // For consistency with the NIST tools, we evaluate the same number of blocks of data, but the size of the block is multiplied by (floor(log(k-1))+1)
    if ((binaryResults = calloc(configRandomRounds * blockCount + 1, sizeof(struct entropyTestingResult))) == NULL) {
      perror("Can't allocate buffer for binary results");
      exit(EX_OSERR);
    }
  } else {
    binaryResults = NULL;
  }

  if(configBlockAssessmentMedian) {
    if ((blockResultsNonIID = calloc(configRandomRounds * blockCount, sizeof(double))) == NULL) {
      perror("Can't allocate buffer for block non-IID results");
      exit(EX_OSERR);
    } else if ((blockResultsIID = calloc(configRandomRounds * blockCount, sizeof(double))) == NULL) {
      free(blockResultsNonIID);
      perror("Can't allocate buffer for block IID results");
      exit(EX_OSERR);
    }
  } else {
      blockResultsNonIID = NULL;
      blockResultsIID = NULL;
  }

  // Note, we do not thread across the round count
  for (size_t i = 0; i < configRandomRounds; i++) {

    // Create random data (if required)
    if (!configUseFile) {
      size_t generationBlocks = configRandDataSize / (evaluationBlockSize*configSerialXOR);

      if (configRingOscillator) {
        double oscFreq = 1000000000;  // Reference RO design is 1GHz
        double oscJitter = (1.0 / oscFreq) * (configJitterPercentage / 100.0);  // Jitter was entered as a percentage per-RO-period.

        assert(oscJitter <= 1 / oscFreq);

        if (configVerbose > 0) {
          if (i == 0) {
            fprintf(stderr, "oscFreq: %.17g\n", oscFreq);
            fprintf(stderr, "Per-sample osc jitter percentage: %.17g\n", configJitterPercentage * sqrt(1000.0));
            fprintf(stderr, "oscJitter: %.17g\n", oscJitter);
            if (configRONu >= 0.0) {
              fprintf(stderr, "sampleFreq: %.17g\n", oscFreq / (1000.0 + configRONu));
            } else {
              fprintf(stderr, "sampleFreq in the interval [%.17g, %.17g]\n", oscFreq / (1000.25), oscFreq / (1000.0));
            }
          }
          fprintf(stderr, "%" PRIdMAX " Generate %zu bits from a simulated ring oscillator for round %zu. ", (intmax_t)time(NULL), configRandDataSize, i + 1);
        }

#pragma omp parallel
        {
          double samplePhase = 0.0;
          double oscPhase;  // Initial phase is random
          struct randstate threadrstate;
          initGenerator(&threadrstate);
          threadrstate.deterministic = rstate.deterministic;
          seedGenerator(&threadrstate);

          // We thread across generationBlocks, so configRandDataSize should be made large to allow for multi threading speedups.
#pragma omp for
          for (size_t l = 0; l < generationBlocks; l++) {
            double localSampleFreq;
            // Each generationBlock reflects data used in a different evaluation.
            oscPhase = randomUnit(&threadrstate);  // Initial phase is random
            if (configRONu < 0.0) {  // if Nu < 0, then we're supposed to randomly vary it randomly.
              double randNu;
              // For modeling, we want the entire phase space [0,.25) explored.
              // Note that divide by 4 only changes the exponent!
              randNu = randomUnit(&threadrstate) / 4.0;
              localSampleFreq = oscFreq / (1000.0 + randNu);  // Reference RO design is sampled near 1MHz.
            } else {
              localSampleFreq = oscFreq / (1000.0 + configRONu);  // Reference RO design is sampled near 1MHz.
            }

            assert(generationBlocks*evaluationBlockSize*configSerialXOR == configRandDataSize);

            for (size_t j = 0; j < evaluationBlockSize*configSerialXOR; j++) {
              data[l*evaluationBlockSize*configSerialXOR + j] = ringOscillatorNextNonDeterministicSample(oscFreq, oscJitter, &oscPhase, localSampleFreq, &samplePhase, &threadrstate);
            }
          }
        } // end parallel
      } else {
        if (configVerbose > 0) fprintf(stderr, "%" PRIdMAX " Generate %zu integers for round %zu. ", (intmax_t)time(NULL), configRandDataSize, i + 1);
#pragma omp parallel
        {
          struct randstate threadrstate;
          initGenerator(&threadrstate);
          threadrstate.deterministic = rstate.deterministic;
          seedGenerator(&threadrstate);

#pragma omp for
          for (size_t l = 0; l < generationBlocks; l++) {
            genRandInts(data + l * evaluationBlockSize*configSerialXOR, evaluationBlockSize*configSerialXOR, (uint32_t)(configK - 1), &threadrstate);
          }
        } //end parallel
      }

      //Do any XORing here
      if(configSerialXOR > 1) {
        size_t localDatalen;
        localDatalen=serialXOR(data, generationBlocks*evaluationBlockSize*configSerialXOR, configSerialXOR);
        assert(localDatalen == datalen);
        assert(configRandDataSize == generationBlocks*evaluationBlockSize*configSerialXOR);
        if(configVerbose > 0) fprintf(stderr, "Performing %zu:1 serial XOR compression. ", configSerialXOR);
      } 

      // Populate bitData
      if (configEval != raw) makeBitstring(data, bitData, datalen, activeBits, configLittleEndian);
    } else {
      if (configVerbose > 0) fprintf(stderr, "File being processed. ");
    }

    if (configVerbose > 0) fprintf(stderr, "Dataset preparation done.\n");

    // All the data is in place now.
    #pragma omp parallel
    {
      if (configEval != bitstring) {
        // We thread across blockCount, so datalen should be made large to allow for multi threading speedups.
        #pragma omp for
        for (size_t j = startIndex; j <= blockCount; j++) {
          if (j != 0)
            doAssessment(data + (j - 1) * evaluationBlockSize, evaluationBlockSize, k, configTestBitmask, rawResults + (i * blockCount) + j, "Literal");
          else
            doAssessment(data, datalen, k, configTestBitmask, rawResults, "Literal");
        }
      } //end literal evaluation

      if (configEval != raw) {
        assert(bitDatalen > 0);
        #pragma omp for
        for (size_t j = startIndex; j <= blockCount; j++) {
          if (j != 0)
            doAssessment(bitData + (j - 1) * bitBlockSize, bitBlockSize, 2, configTestBitmask, binaryResults + (i * blockCount) + j, "Bitstring");
          else
            doAssessment(bitData, bitDatalen, 2, configTestBitmask, binaryResults, "Bitstring");
        }
      } //end bitstring evaluation
    } //end parallel region

  } // round for loop

  if (configVerbose > 0) fprintf(stderr, "Done with calculation\n\n");

  // output results
  for (size_t j = 1; j <= configRandomRounds * blockCount; j++) {
    double minminent = DBL_INFINITY;
    double minIIDminent = DBL_INFINITY;

    if ((configRandomRounds * blockCount > 1) || configLargeBlockAssessment)
      fprintf(stderr, "Results for block %zu\n", j);
    else
      fprintf(stderr, "Results for sole block\n");

    if (configEval != bitstring) {
      printEntropyTestingResult(rawResults + j);
      minminent = rawResults[j].assessedEntropy;
      minIIDminent = rawResults[j].assessedIIDEntropy;
      printf("H_original = %.17g\n", rawResults[j].assessedEntropy);
      printf("H_original (IID) = %.17g\n", rawResults[j].assessedIIDEntropy);
      fflush(stdout);
    }

    if (configEval != raw) {
      printEntropyTestingResult(binaryResults + j);
      printf("H_bitstring = %.17g\n", binaryResults[j].assessedEntropy);
      printf("H_bitstring Per Symbol = %.17g\n", (double)bitWidth * binaryResults[j].assessedEntropy);
      minminent = fmin(minminent, (double)bitWidth * binaryResults[j].assessedEntropy);

      printf("H_bitstring (IID) = %.17g\n", binaryResults[j].assessedIIDEntropy);
      printf("H_bitstring Per Symbol (IID) = %.17g\n", (double)bitWidth * binaryResults[j].assessedIIDEntropy);
      minIIDminent = fmin(minIIDminent, (double)bitWidth * binaryResults[j].assessedIIDEntropy);
      fflush(stdout);
    }

    printf("Assessed min entropy = %.17g\n", minminent);
    printf("Assessed min entropy (IID) = %.17g\n\n", minIIDminent);

    //Remember the overall results for later processing
    if(configBlockAssessmentMedian) {
      blockResultsNonIID[j-1] = minminent;
      blockResultsIID[j-1] = minIIDminent;
    }

    fflush(stdout);
  }

  if(configBlockAssessmentMedian) {
    double medianNonIIDAssessment, medianIIDAssessment;
    double closestAssessmentDistance = (double)bitWidth;
    size_t closestAssessmentIndex = configRandomRounds * blockCount;
    //The BCaBootstrapPercentile call sorts the data as a side effect, so we'll copy it first.
    double *blockResultsNonIIDSorted, *blockResultsIIDSorted;

    if(configVerbose > 0) fprintf(stderr, "Calculating block median assessment\n");

    if((blockResultsNonIIDSorted = malloc(sizeof(double)*configRandomRounds * blockCount))==NULL) {
      perror("Can't allocate buffer for block sorted non-IID results");
      exit(EX_OSERR);
    }
    memcpy(blockResultsNonIIDSorted, blockResultsNonIID, sizeof(double)*configRandomRounds * blockCount);

    medianNonIIDAssessment = BCaBootstrapPercentile(0.5, blockResultsNonIIDSorted, configRandomRounds * blockCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, &rstate);
    printf("Median Assessed min entropy = %.17g\n", medianNonIIDAssessment);
    if(configVerbose > 0) fprintf(stderr, "Following blocks have assessments within median confidence interval: ");
    for (size_t j = 0; j < configRandomRounds * blockCount; j++) {
      double curAssessmentDistance = fabs(blockResultsNonIID[j] - medianNonIIDAssessment);
      if(curAssessmentDistance < closestAssessmentDistance) {
        closestAssessmentDistance = curAssessmentDistance;
        closestAssessmentIndex = j;
      }

      if((configVerbose > 0) && (blockResultsNonIID[j] >= confidenceInterval[0]) && (blockResultsNonIID[j] <= confidenceInterval[1])) fprintf(stderr, "%zu ", j+1);
    }

    if(configVerbose > 0) fprintf(stderr, "\nClosest block assessment is %.17g for block %zu (delta is %.17g)\n", blockResultsNonIID[closestAssessmentIndex], closestAssessmentIndex+1, closestAssessmentDistance);
    free(blockResultsNonIIDSorted);
    blockResultsNonIIDSorted = NULL;

    if(configVerbose > 0) fprintf(stderr, "Calculating block median IID assessment\n");
    if((blockResultsIIDSorted = malloc(sizeof(double)*configRandomRounds * blockCount))==NULL) {
      perror("Can't allocate buffer for block sorted IID results");
      exit(EX_OSERR);
    }
    memcpy(blockResultsIIDSorted, blockResultsIID, sizeof(double)*configRandomRounds * blockCount);

    medianIIDAssessment = BCaBootstrapPercentile(0.5, blockResultsIIDSorted, configRandomRounds * blockCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, &rstate);
    printf("Median Assessed min entropy (IID) = %.17g\n", medianIIDAssessment);
    if(configVerbose > 0) fprintf(stderr, "Following blocks have assessments within median confidence interval: ");
    for (size_t j = 0; j < configRandomRounds * blockCount; j++) {
      double curAssessmentDistance = fabs(blockResultsIID[j] - medianIIDAssessment);
      if(curAssessmentDistance < closestAssessmentDistance) {
        closestAssessmentDistance = curAssessmentDistance;
        closestAssessmentIndex = j;
      }

      if((configVerbose > 0) && (blockResultsIID[j] >= confidenceInterval[0]) && (blockResultsIID[j] <= confidenceInterval[1])) fprintf(stderr, "%zu ", j+1);
    }

    if(configVerbose > 0) fprintf(stderr, "\nClosest block assessment is %.17g for block %zu (delta is %.17g)\n", blockResultsIID[closestAssessmentIndex], closestAssessmentIndex+1, closestAssessmentDistance);
    free(blockResultsIIDSorted);
    blockResultsIIDSorted = NULL;

    printf("\n");
  }
  fflush(stdout);

  // Find a final assessment (if applicable)
  if (configLargeBlockAssessment || configBootstrapParams || configBootstrapAssessments) {
    double minminent = DBL_INFINITY;
    double minIIDminent = DBL_INFINITY;

    if (configLargeBlockAssessment) {
      double localMinminent = DBL_INFINITY;
      double localMinIIDminent = DBL_INFINITY;
      fprintf(stderr, "Results for Large Block Assessment\n");
      if (configEval != bitstring) {
        printEntropyTestingResult(rawResults);
        localMinminent = rawResults[0].assessedEntropy;
        fprintf(stderr, "Large Block Assessment H_original = %.17g\n", rawResults[0].assessedEntropy);

        localMinIIDminent = rawResults[0].assessedIIDEntropy;
        fprintf(stderr, "Large Block Assessment H_original (IID) = %.17g\n", rawResults[0].assessedIIDEntropy);

        fflush(stdout);
      }

      if (configEval != raw) {
        printEntropyTestingResult(binaryResults);
        fprintf(stderr, "Large Block Assessment H_bitstring = %.17g\n", binaryResults[0].assessedEntropy);
        fprintf(stderr, "Large Block Assessment H_bitstring Per Symbol= %.17g\n", (double)bitWidth * binaryResults[0].assessedEntropy);
        localMinminent = fmin(localMinminent, (double)bitWidth * binaryResults[0].assessedEntropy);

        fprintf(stderr, "Large Block Assessment H_bitstring (IID) = %.17g\n", binaryResults[0].assessedIIDEntropy);
        fprintf(stderr, "Large Block Assessment H_bitstring Per Symbol (IID) = %.17g\n", (double)bitWidth * binaryResults[0].assessedIIDEntropy);
        localMinIIDminent = fmin(localMinIIDminent, (double)bitWidth * binaryResults[0].assessedIIDEntropy);

        fflush(stdout);
      }

      printf("Large Block Assessment min entropy = %.17g\n", localMinminent);
      printf("Large Block Assessment min entropy (IID) = %.17g\n\n", localMinIIDminent);
      fflush(stdout);

      minminent = fmin(minminent, localMinminent);
      minIIDminent = fmin(minIIDminent, localMinIIDminent);
    }

    if (configBootstrapAssessments) {
      double localMinminent = DBL_INFINITY;
      double localMinIIDminent = DBL_INFINITY;

      fprintf(stderr, "Starting Assessment Bootstrap\n");
      if (configEval != bitstring) {
        double testRes;
        double IIDtestRes;

        testRes = bootstrapAssessments(rawResults + 1, configRandomRounds * blockCount, bitWidth, &IIDtestRes, &rstate);
        localMinminent = testRes;
	localMinIIDminent = IIDtestRes;
        printf("Assessment Bootstrap H_original = %.17g\n", testRes);
        printf("Assessment Bootstrap H_original (IID) = %.17g\n", IIDtestRes);
        fflush(stdout);
      }

      if (configEval != raw) {
        double testRes;
        double IIDtestRes;

        testRes = bootstrapAssessments(binaryResults + 1, configRandomRounds * blockCount, 1, &IIDtestRes, &rstate);
        printf("Assessment Bootstrap H_bitstring = %.17g\n", testRes);
        printf("Assessment Bootstrap H_bitstring Per Symbol = %.17g\n", (double)bitWidth * testRes);
        localMinminent = fmin(localMinminent, (double)bitWidth * testRes);

        printf("Assessment Bootstrap H_bitstring (IID) = %.17g\n", IIDtestRes);
        printf("Assessment Bootstrap H_bitstring Per Symbol (IID) = %.17g\n", (double)bitWidth * IIDtestRes);
        localMinIIDminent = fmin(localMinIIDminent, (double)bitWidth * IIDtestRes);

        fflush(stdout);
      }

      printf("Assessment Bootstrap min entropy = %.17g\n", localMinminent);
      printf("Assessment Bootstrap min entropy (IID) = %.17g\n\n", localMinIIDminent);
      fflush(stdout);

      minminent = fmin(minminent, localMinminent);
      minIIDminent = fmin(minIIDminent, localMinIIDminent);
    }

    if (configBootstrapParams) {
      double localMinminent = DBL_INFINITY;
      double localMinIIDminent = DBL_INFINITY;

      fprintf(stderr, "Starting Parameter Bootstrap\n");
      if (configEval != bitstring) {
        double testRes;
        double IIDtestRes;

        testRes = bootstrapParameters(rawResults + 1, configRandomRounds * blockCount, bitWidth, &IIDtestRes, &rstate);
        localMinminent = testRes;
        localMinIIDminent = IIDtestRes;
        printf("Parameter Bootstrap H_original = %.17g\n", testRes);
        printf("Parameter Bootstrap H_original (IID) = %.17g\n", IIDtestRes);
        fflush(stdout);
      }

      if (configEval != raw) {
        double testRes;
        double IIDtestRes;

        testRes = bootstrapParameters(binaryResults + 1, configRandomRounds * blockCount, 1, &IIDtestRes, &rstate);
        printf("Parameter Bootstrap H_bitstring = %.17g\n", testRes);
        printf("Parameter Bootstrap H_bitstring Per Symbol = %.17g\n", (double)bitWidth * testRes);
        localMinminent = fmin(localMinminent, (double)bitWidth * testRes);

        printf("Parameter Bootstrap H_bitstring (IID) = %.17g\n", IIDtestRes);
        printf("Parameter Bootstrap H_bitstring Per Symbol (IID) = %.17g\n", (double)bitWidth * IIDtestRes);
        localMinIIDminent = fmin(localMinIIDminent, (double)bitWidth * IIDtestRes);

        fflush(stdout);
      }

      printf("Parameter Bootstrap min entropy = %.17g\n", localMinminent);
      printf("Parameter Bootstrap min entropy (IID) = %.17g\n\n", localMinIIDminent);
      fflush(stdout);

      minminent = fmin(minminent, localMinminent);
      minIIDminent = fmin(minIIDminent, localMinIIDminent);
    }

    printf("Final Assessment = %.17g\n", minminent);
    printf("Final Assessment (IID) = %.17g\n", minIIDminent);
    fflush(stdout);
  }

  if (data != NULL) {
    free(data);
    data = NULL;
  }
  if (bitData != NULL) {
    free(bitData);
    bitData = NULL;
  }
  if (rawResults != NULL) {
    free(rawResults);
    rawResults = NULL;
  }
  if (binaryResults != NULL) {
    free(binaryResults);
    binaryResults = NULL;
  }
  if (blockResultsNonIID != NULL) {
    free(blockResultsNonIID);
    blockResultsNonIID = NULL;
  }
  if (blockResultsIID != NULL) {
    free(blockResultsIID);
    blockResultsIID = NULL;
  }

  return 0;
}
