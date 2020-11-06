/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef ENTLIB_H
#define ENTLIB_H

#include <stdbool.h>
#include <stdint.h>
#include "enttypes.h"

#ifdef U32STATDATA
#define statData_t uint32_t
#define signedStatData_t int64_t
#define STATDATA_MAX UINT32_MAX
#define STATDATA_BITS 32
#define STATDATA_STRING "uint32_t"
#else
#define statData_t uint8_t
#define signedStatData_t int16_t
#define STATDATA_MAX UINT8_MAX
#define STATDATA_BITS 8
#define STATDATA_STRING "uint8_t"
#endif

#define MCVESTIMATEMASK 0x01
#define COLSESTIMATEMASK 0x02
#define MARKOVESTIMATEMASK 0x04
#define COMPESTIMATEMASK 0x08
#define SAESTIMATEMASK 0x10
#define MCWESTIMATEMASK 0x20
#define LAGESTIMATEMASK 0x40
#define TREEMMCESTIMATEMASK 0x80
#define TREELZ78YESTIMATEMASK 0x0100
#define NSAMARKOVESTIMATEMASK 0x0200

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Note, these are used as array indices
// The C11 standard defines this as starting with 0.
enum entropyEstimators { MCVest, colsEst, markovEst, compEst, SAest, MCWest, LAGest, MMCest, LZ78Yest, NSAmarkovEst };

/*SP800-90B-final 6.3.1*/
struct MCVresult {
  bool done;
  size_t maxCount;
  double phat;  // overall assessment based on upper bound for 99% confidence interval of phat
  double pu;
  double entropy;
  double runTime;
};

double mostCommonValueEstimate(const statData_t *S, size_t L, size_t k, struct MCVresult *result);

/*SP800-90B-final 6.3.2*/

struct colsResult {
  bool done;
  size_t v;
  size_t tSum;
  double mean;  // take the lower 99% confidence interval bound of this parameter
  double stddev;
  double meanbound;
  double p;
  double entropy;
  double runTime;
};

double collisionEstimate(const statData_t *S, size_t L, struct colsResult *result);

/*SP800-90B-final 6.3.3*/
struct markovResult {
  bool done;
  double P0;  // Take the upper 99% confidence bound
  double P1;  // Take the upper 99% confidence bound
  double T[2][2];  // Take the upper 99% confidence bound
  double phatmax;
  double entropy;
  double runTime;
};

double markovEstimate(const statData_t *S, size_t L, struct markovResult *result);

/*SP800-90B-final 6.3.4*/

struct compResult {
  bool done;
  double stddev;
  double meanbound;
  double mean;  // Take the lower 99% confidence interval bound of this parameter
  double p;
  double entropy;
  double runTime;
  size_t L;
};
double compressionEstimate(const statData_t *S, size_t L, struct compResult *result);

/*SP800-90B-final 6.3.5 and 6.3.6*/

struct SAresult {
  bool done;
  bool tTupleDone;
  int32_t u;  // t = u-1
  int32_t v;
  double tTuplePmax;  // Take the upper 99% confidence interval of this parameter
  double tTuplePu;
  double tTupleEntropy;
  bool lrsDone;
  double lrsPmax;  // Take the upper 99% confidence interval of this parameter
  double lrsPu;
  double lrsEntropy;
  double runTime;
};

void SAalgs(const statData_t *S, size_t L, size_t k, struct SAresult *result);

/*SP800-90B-final 6.3.7 to 6.3.10*/
/*For all prediction estimates, note take the upper 99% confidence interval bound for Pglobal, Plocal*/
struct predictorResult {
  bool done;
  size_t C;
  size_t r;
  size_t N;
  size_t k;
  double Pglobal;  // If C > 0, take the upper 99% confidence interval
  double PglobalBound;
  double Plocal;  // take the upper 99% confidence interval
  double Prun; //This is the probability of seeing this r with this P_global
  double entropy;  //-log2(Max(PlocalBound, PglobalBound, 1/k)
  double runTime;
};

double multiMCWPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result);
double lagPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result);
double treeMultiMMCPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result);
double treeLZ78YPredictionEstimate(const statData_t *S, size_t L, size_t k, struct predictorResult *result);

double calcPglobalBound(double pGlobal, size_t N);
double calcPlocal(size_t N, size_t r, size_t k, double runningMax, size_t rounds, bool noSkip);
double calcPrun(double Pglobal, size_t N, size_t r);

/*Now retired, be we still use it*/
double NSAMarkovEstimate(const statData_t *S, size_t L, size_t k, const char *label, bool conservative, double probCutoff);

struct entropyTestingResult {
  char label[16];  //"Literal" or "Bitstring"
  struct MCVresult mcv;
  struct colsResult cols;
  struct markovResult markov;
  struct compResult comp;
  struct SAresult sa;
  struct predictorResult mcw;
  struct predictorResult lag;
  struct predictorResult mmc;
  struct predictorResult lz78y;
  double assessedEntropy;
  double runTime;
};

void printEntropyTestingResult(const struct entropyTestingResult *result);
void initEntropyTestingResult(const char *label, struct entropyTestingResult *result);
double shannonEntropyEstimate(const statData_t *S, size_t L, size_t k);
#endif
