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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "fancymath.h"
#include "globals-inst.h"
#include "precision.h"
#include "randlib.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "randomfile [-v] [-b <p>,<bits>] [-s <m>] [-d] [-a <andmask>] [-n <mean>,<stddev>]\n");
  fprintf(stderr, "-d \t Make any RNG input deterministic.\n");
  fprintf(stderr, "-b <p>,<b> \t Produce b-bit symbols with Pr(0) = p, of each other symbol is (1-p)/(2^b - 1).\n");
  fprintf(stderr, "-c <c> \t Produce 1-bit symbols with correlation c, that is Pr(X_j = a| X_{j-1} = a) = (c+1)/2 (so -1 <= c <= 1).\n");
  fprintf(stderr, "-n <mean>,<stddev>,<bits> \t Produce truncated normally distributed samples, with the provided mean and standard deviation, fitting into <bits> bits.\n");
  fprintf(stderr, "-u <startBias>,<serialCoefficient>,<stepNoiseMean>,<stepNoiseStdDev>,<leftStepSize>,<rightStepSize> \t Produce bitwise data using the SUMS model.\n");
  fprintf(stderr, "-p <magnitude>,<period> \t Sinusoidal bias, with <magnitude> and <period> listed (each sample takes 1 time unit).\n");
  fprintf(stderr, "-s <m> \t Use a sample set of <m> values. (default m=1000000)\n");
  fprintf(stderr, "-f <b>,<p> \t Output <b> bits from an output filtered via a LFSR (<p> is the LFSR)\n");
  fprintf(stderr, "-a <andmask> \t AND the output with andmask\n");
  fprintf(stderr, "-l <len>\tlength of the averaging block.\n");
  fprintf(stderr, "-v \tverbose mode.\n");
  fprintf(stderr, "-t \tOutput " STATDATA_STRING " integers.\n");
  fprintf(stderr, "outputs random uint32_t (" STATDATA_STRING " when \"-t\" option is used) integers to stdout\n");
  exit(EX_USAGE);
}

struct ringBufferState {
  double *ringBuf;
  size_t ringSize;
  size_t ringStart;
  size_t ringEnd;
  bool fullRing;
  double runningSum;
  double ringSum;
  double maxMinEnt;
  double minMinEnt;
};

static struct ringBufferState *newRing(size_t len) {
  struct ringBufferState *newRing;
  size_t j;

  assert(len > 0);

  if ((newRing = malloc(sizeof(struct ringBufferState))) == NULL) {
    perror("Can't allocate new ring buffer structure");
    exit(EX_OSERR);
  }

  if ((newRing->ringBuf = malloc(sizeof(double) * len)) == NULL) {
    perror("Can't allocate new ring buffer structure");
    exit(EX_OSERR);
  }

  newRing->ringSize = len;
  newRing->ringStart = newRing->ringEnd = 0;
  newRing->fullRing = false;
  newRing->runningSum = 0.0;
  newRing->ringSum = newRing->maxMinEnt = newRing->minMinEnt = DBL_INFINITY;

  for (j = 0; j < len; j++) {
    newRing->ringBuf[j] = DBL_INFINITY;
  }

  return newRing;
}

static void delRing(struct ringBufferState *ring) {
  assert(ring != NULL);
  assert(ring->ringBuf != NULL);

  free(ring->ringBuf);
  free(ring);
}

static bool addToRing(double in, struct ringBufferState *ring) {
  size_t newEndLoc;
  size_t j;

  assert(ring != NULL);
  assert(in < DBL_INFINITY);

  // The new end for the next data
  newEndLoc = (ring->ringEnd + 1) % (ring->ringSize);

  if (ring->fullRing) {
    assert(ring->ringEnd == ring->ringStart);
    // We are going to displace a value at the end
    ring->ringSum -= ring->ringBuf[ring->ringEnd];
    ring->ringSum += in;
    // Now check how this compares to the existing min / max
    if (ring->ringSum > ring->maxMinEnt) ring->maxMinEnt = ring->ringSum;
    if (ring->ringSum < ring->minMinEnt) ring->minMinEnt = ring->ringSum;

    // The ring is full, so start gets updated too
    ring->ringStart = newEndLoc;
  } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
    assert(ring->ringBuf[ring->ringEnd] == DBL_INFINITY);
#pragma GCC diagnostic pop
  }

  // Fill in the new data
  ring->ringBuf[ring->ringEnd] = in;
  // update ringEnd to point to the new end
  ring->ringEnd = newEndLoc;
  ring->runningSum += in;

  if (!ring->fullRing && (ring->ringStart == ring->ringEnd)) {
    // We just filled up the ring for the first time
    ring->fullRing = true;
    ring->ringSum = 0.0;
    for (j = 0; j < ring->ringSize; j++) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
      assert(ring->ringBuf[j] != DBL_INFINITY);
#pragma GCC diagnostic pop
      ring->ringSum += ring->ringBuf[j];
    }
    ring->maxMinEnt = ring->minMinEnt = ring->ringSum;
  }

  return (ring->fullRing);
}

int main(int argc, char *argv[]) {
  size_t configOutputLen;
  double configMean, configStdDev;
  uint32_t configAndMask;
  uint32_t configSymbolBits;
  double configBias;
  size_t i;
  struct randstate rstate;
  struct SUMSstate sstate = {.bias = 0.0, .serialCoefficient = 0.0, .stepNoiseMean = 0.0, .stepNoiseStdDev = 0.0, .leftStepSize = 0.1, .rightStepSize = 0.1, .lastOutput = UINT32_MAX, .lastEntropy = -DBL_INFINITY};
  struct sinBiasState biasState = {.magnitude = 1.0, .period = 8.0, .currentPhase = 0.0, .lastEntropy = -DBL_INFINITY};
  uint32_t out;
  long inparam;
  int opt;
  char *nextOption;
  double indouble;
  unsigned long long int inint;
  double configCorrelationConstant;
  bool configCorrelated;
  bool configSUMS;
  bool configNormal;
  bool configBiased;
  bool configOutputStatData;
  statData_t *sdout;
  uint32_t *data;
  struct ringBufferState *ring;
  size_t configRingLen;
  bool configSinBias;
  uint64_t configLFSRPoly;
  uint64_t LFSRState;
  uint32_t configLFSROutput;

  assert(PRECISION(UINT_MAX) >= 32);
  assert(PRECISION((unsigned char)UCHAR_MAX) == 8);

  configOutputLen = 1000000;
  configVerbose = 0;
  configNormal = false;
  configMean = 1024.0;
  configStdDev = 128.0;
  configAndMask = 0xFFFFFFFF;
  configSymbolBits = 32;
  configBias = 1.0 / ((double)((uint64_t)1 << configSymbolBits));
  configCorrelationConstant = 0.0;
  configCorrelated = false;
  configSUMS = false;
  configBiased = true;
  configRingLen = 128;
  configSinBias = false;
  configLFSRPoly = 0;
  configLFSROutput = 0;
  LFSRState = 1;
  configOutputStatData = false;

  initGenerator(&rstate);

  while ((opt = getopt(argc, argv, "vdb:c:n:u:s:a:l:p:f:tm:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 't':
        configOutputStatData = true;
        break;
      case 's':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0)) {
          useageExit();
        }
        configOutputLen = (uint32_t)inparam;
        break;
      case 'l':
        inparam = strtol(optarg, NULL, 10);
        if ((inparam <= 0) || (inparam > 1000000)) {
          useageExit();
        }
        configRingLen = (size_t)inparam;
        break;
      case 'd':
        rstate.deterministic = true;
        break;
      case 'p':
        configSinBias = true;
        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          fprintf(stderr, "first\n");
          useageExit();
        }
        biasState.magnitude = indouble;
        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, NULL);
        if ((indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
          fprintf(stderr, "second: optarg (%s)\n", optarg);
          useageExit();
        }
        biasState.period = indouble;
        break;
      case 'f':
        inint = strtoull(optarg, &nextOption, 0);
        if ((nextOption == optarg) || (inint == 0) || (inint > 32) || (errno == EINVAL) || (*nextOption != ',')) {
          fprintf(stderr, "LFSR first\n");
          useageExit();
        }
        nextOption++;
        configLFSROutput = (uint32_t)inint;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == 0) || (inint > UINT64_MAX) || (errno == EINVAL)) {
          fprintf(stderr, "LFSR second\n");
          useageExit();
        }
        configLFSRPoly = (uint32_t)inint;
        break;
      case 'n':
        configNormal = true;
        configCorrelated = false;
        configSUMS = false;
        configBiased = false;
        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          fprintf(stderr, "first\n");
          useageExit();
        }
        configMean = indouble;
        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          fprintf(stderr, "second: optarg (%s) nextOption (%c)\n", optarg, *nextOption);
          useageExit();
        }
        nextOption++;
        configStdDev = indouble;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == 0) || (inint > 32) || (errno == EINVAL)) {
          fprintf(stderr, "third\n");
          useageExit();
        }
        configSymbolBits = (uint32_t)inint;
        break;
      case 'a':
        inint = strtoull(optarg, NULL, 0);
        if ((inint > 0) && (inint <= (long long int)UINT_MAX)) {
          configAndMask = (uint32_t)inint;
        } else {
          useageExit();
        }
        break;
      case 'c':
        configCorrelated = true;
        configSUMS = false;
        configNormal = false;
        configBiased = false;

        indouble = strtod(optarg, NULL);
        if ((indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < -1.0) || (indouble > 1.0)) {
          useageExit();
        }

        configCorrelationConstant = indouble;
        break;
      case 'u':
        configCorrelated = false;
        configSUMS = true;
        configNormal = false;
        configBiased = false;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        sstate.bias = indouble;

        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        sstate.serialCoefficient = indouble;

        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        sstate.stepNoiseMean = indouble;

        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        sstate.stepNoiseStdDev = indouble;

        nextOption++;
        optarg = nextOption;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        sstate.leftStepSize = indouble;

        nextOption++;
        optarg = nextOption;

        indouble = strtod(nextOption, NULL);
        if ((indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
          useageExit();
        }

        sstate.rightStepSize = indouble;
        break;
      case 'b':
        configCorrelated = false;
        configSUMS = false;
        configNormal = false;
        configBiased = true;

        indouble = strtod(optarg, &nextOption);
        if ((nextOption == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (*nextOption != ',')) {
          useageExit();
        }
        configBias = indouble;

        nextOption++;

        inint = strtoull(nextOption, NULL, 0);
        if ((inint == 0) || (inint > 32) || (errno == EINVAL)) {
          useageExit();
        }
        configSymbolBits = (uint32_t)inint;
        break;
      default: /* '?' */
        useageExit();
    }
  }

  seedGenerator(&rstate);

  if (configSinBias) {
    biasState.currentPhase = randomUnit(&rstate);
  }

  assert(configSymbolBits <= 32);

  if (configBiased && (configBias < (1.0 - configBias) / ((double)(((uint64_t)1U << configSymbolBits) - 1)))) {
    if (configVerbose > 0) fprintf(stderr, "Bias set too low. Resetting to unbiased\n");
    configBias = (1.0) / ((double)((uint64_t)1U << configSymbolBits));
  }

  if (configVerbose > 0) {
    if (configNormal) {
      fprintf(stderr, "Outputting normal data\n");
      if (configSinBias) {
        fprintf(stderr, "Using a Sinusoidal bias magnitude: %.17g, period: %.17g, starting Phase: %.17g\n", biasState.magnitude, biasState.period, biasState.currentPhase);
        fprintf(stderr, "Mean: %.17g StdDev: %.17g, Bit width: %u\n", configMean, configStdDev, configSymbolBits);

      } else {
        fprintf(stderr, "Mean: %.17g StdDev: %.17g, Bit width: %u, min entropy: %.17g\n", configMean, configStdDev, configSymbolBits, truncatedNormalminEnt(configMean, configStdDev, configSymbolBits));
      }
    } else if (configBiased) {
      fprintf(stderr, "Outputting biased data\n");
      fprintf(stderr, "Output bits: %u, Bias: %.17g, Entropy: %.17g\n", configSymbolBits, configBias, -log(configBias) / log(2.0));
    } else if (configSUMS) {
      fprintf(stderr, "Outputting SUMS data\n");
      fprintf(stderr, "Bias: %.17g, Serial Coefficient: %.17g, Step Noise Mean: %.17g, Step Noise Std Dev: %.17g, Left Step Size: %.17g, Right Step Size: %.17g.\n", sstate.bias, sstate.serialCoefficient, sstate.stepNoiseMean, sstate.stepNoiseStdDev,
              sstate.leftStepSize, sstate.rightStepSize);
    } else if (configCorrelated) {
      fprintf(stderr, "Outputting correlated data\n");
      fprintf(stderr, "Correlation Constant: %.17g, Min Entropy: %.17g\n", configCorrelationConstant, -log(fmax((configCorrelationConstant + 1.0) / 2.0, 1.0 - (configCorrelationConstant + 1.0) / 2.0)) / log(2.0));
    } else {
      fprintf(stderr, "Unknown mode\n");
      exit(EX_DATAERR);
    }
  }

  if (configVerbose > 0) {
    fprintf(stderr, "Outputting %zu uint32_t values\n", configOutputLen);
  }

  out = 0;
  ring = newRing(configRingLen);

  if ((data = malloc(sizeof(uint32_t) * configOutputLen)) == NULL) {
    perror("Can't allocate data for internal output buffer");
    exit(EX_OSERR);
  }

  for (i = 0; i < configOutputLen; i++) {
    if (configNormal) {
      if (configSinBias) {
        out = genNormalInt(configMean, configStdDev, configSymbolBits, &biasState, &rstate);
        if (configVerbose > 3) fprintf(stderr, "min entropy: %.17g\n", biasState.lastEntropy);
        addToRing(biasState.lastEntropy, ring);
      } else {
        out = genNormalInt(configMean, configStdDev, configSymbolBits, NULL, &rstate);
      }
    } else if (configBiased) {
      out = genRandBiasedInt(configSymbolBits, configBias, &rstate);
    } else if (configSUMS) {
      out = SUMSgenerate(&sstate, &rstate);
      if (configVerbose > 3) fprintf(stderr, "min entropy: %.17g\n", sstate.lastEntropy);
      if (configVerbose > 4) fprintf(stderr, "bias: %.17g\n", sstate.bias);
      addToRing(sstate.lastEntropy, ring);
    } else if (configCorrelated) {
      out = genRandCorrelatedBit(configCorrelationConstant, out, &rstate);
    } else {
      fprintf(stderr, "Unknown mode\n");
      exit(EX_DATAERR);
    }

    if (configLFSRPoly != 0) {
      out = LFSRFilter(out + configLFSROutput, configLFSROutput, &LFSRState, configLFSRPoly);
    }

    out &= configAndMask;

    data[i] = out;
  }

  // For SUMS data entropy estimate
  if ((configVerbose > 0) && configSUMS) {
    fprintf(stderr, "SUMS Min Entropy Min: %.17g, Min Entropy Max: %.17g, Min Entropy Ave: %.17g\n", ring->minMinEnt / ((double)(ring->ringSize)), ring->maxMinEnt / ((double)(ring->ringSize)), ring->runningSum / (double)configOutputLen);
  } else if ((configVerbose > 0) && configNormal && configSinBias) {
    fprintf(stderr, "Normal Sin Biased Min Entropy Min: %.17g, Min Entropy Max: %.17g, Min Entropy Ave: %.17g\n", ring->minMinEnt / ((double)(ring->ringSize)), ring->maxMinEnt / ((double)(ring->ringSize)), ring->runningSum / (double)configOutputLen);
  }

  delRing(ring);

  // Now output the data
  if (configOutputStatData) {
    if ((sdout = malloc(sizeof(statData_t) * configOutputLen)) == NULL) {
      perror("Can't allocate statData output buffer");
      exit(EX_OSERR);
    }
    for (i = 0; i < configOutputLen; i++) {
      assert(data[i] <= STATDATA_MAX);
      sdout[i] = (statData_t)data[i];
    }
    if (fwrite(sdout, sizeof(statData_t), configOutputLen, stdout) != configOutputLen) {
      perror("Can't write statData output");
      exit(EX_OSERR);
    }
    free(sdout);
  } else {
    if (fwrite(data, sizeof(uint32_t), configOutputLen, stdout) != configOutputLen) {
      perror("Can't write u32 data output");
      exit(EX_OSERR);
    }
  }

  free(data);

  return EX_OK;
}
