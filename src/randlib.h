/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef RANDLIB_H
#define RANDLIB_H
#include <stdbool.h>
#include <stdint.h>
#include "SFMT.h"
#include "enttypes.h"

struct randstate {
  bool seeded;
  bool deterministic;
  bool buffered32Avail;
  uint32_t buffered32;
  sfmt_t sfmt;
  uint64_t xoshiro256starstarState[4];
};

struct SUMSstate {
  double bias;
  double serialCoefficient;
  double stepNoiseMean;
  double stepNoiseStdDev;
  double leftStepSize;
  double rightStepSize;
  uint32_t lastOutput;
  double lastEntropy;
};

struct MBMstate {
  double A;
  double R;
  double sigma;
  double limit;
  double V;
  uint32_t lastOutput;
  double lastEntropy;
  size_t noiseFlips;
};

struct processedMBMstate {
  bool initilized;
  size_t aCurIndex;
  uint32_t aOutputs[3];
  double aEntropies[3];
  uint32_t lastOutput;
  double lastEntropy;
};

struct sinBiasState {
  double magnitude;
  double period;
  double currentPhase;
  double lastEntropy;
};

void initGenerator(struct randstate *rstate);
void seedGenerator(struct randstate *rstate);
uint64_t randomu64(struct randstate *rstate);
uint32_t randomu32(struct randstate *rstate);
uint32_t randomRange(uint32_t high, struct randstate *rstate);
uint64_t randomRange64(uint64_t high, struct randstate *rstate);
uint32_t genRandBiasedBit(double bias, struct randstate *rstate);
unsigned char *genRandBitBytes(double bias, size_t datalen, struct randstate *rstate);
void genRandInts(statData_t *data, size_t datalen, uint32_t k, struct randstate *rstate);
double randomSignedUnit(struct randstate *rstate);
double randomUnit(struct randstate *rstate);
void normalVariate(double mean, double stdDev, double *out1, double *out2, struct randstate *rstate);
double truncatedNormalminEnt(double mean, double stddev, uint32_t bits);
uint32_t genNormalInt(double mean, double stddev, uint32_t bits, struct sinBiasState *bstate, struct randstate *rstate);
uint32_t SUMSgenerate(struct SUMSstate *sstate, struct randstate *rstate);
uint32_t genRandCorrelatedBit(double c, uint32_t lastbit, struct randstate *rstate);
uint32_t genRandBiasedInt(uint32_t bits, double p, struct randstate *rstate);
uint32_t LFSRFilter(uint32_t input, uint32_t bits, uint64_t *state, uint64_t polynomial);
void FYShuffle(double *indata, size_t datalen, struct randstate *rstate);
statData_t ringOscillatorNextDeterministicSample(double oscFreq, double oscPhase, double sampleFreq, double samplePhase);
statData_t ringOscillatorNextNonDeterministicSample(double oscFreq, double oscJitter, double *relOscPhase, double sampleFreq, double *relSamplePhase, struct randstate *rstate);
void randomBits(statData_t *data, size_t datalen, struct randstate *rstate);
uint8_t genRandBit(double p, struct randstate *rstate);
#endif
