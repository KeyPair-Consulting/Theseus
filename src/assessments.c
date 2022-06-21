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
#include <stddef.h>

#include "assessments.h"
#include "bootstrap.h"
#include "entlib.h"
#include "enttypes.h"
#include "randlib.h"

double bootstrapAssessments(struct entropyTestingResult *results, size_t count, size_t bitWidth, double *IIDminent, struct randstate *rstate) {
  double *entropyResults;
  double confidenceInterval[2];
  double minminent = DBL_INFINITY;

  assert(results != NULL);
  assert(count > 0);
  assert(bitWidth > 0);
  assert(bitWidth <= STATDATA_BITS);
  assert(rstate != NULL);
  assert(rstate->seeded);

  if(IIDminent != NULL) *IIDminent = -1.0;

  if ((entropyResults = malloc(sizeof(double) * count)) == NULL) {
    perror("Can't allocate memory for result data");
    exit(EX_OSERR);
  }

  // 6.3.1
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].mcv.done) {
        entropyResults[localCount] = results[i].mcv.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s Most Common Value Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s Most Common Value Estimate: min entropy = %.17g\n", results[0].label, testRes);
      if(IIDminent != NULL) *IIDminent = testRes;
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.2
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].cols.done) {
        entropyResults[localCount] = results[i].cols.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s Collision Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s Collision Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.3
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].markov.done) {
        entropyResults[localCount] = results[i].markov.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s Markov Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.005, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s Markov Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.4
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].comp.done) {
        entropyResults[localCount] = results[i].comp.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s Compression Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s Compression Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.5 and 6.3.6
  {
    size_t localCount = 0;

    // 6.3.5
    for (size_t i = 0; i < count; i++) {
      if (results[i].sa.tTupleDone) {
        entropyResults[localCount] = results[i].sa.tTupleEntropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s t-Tuple Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s t-Tuple Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }

    // 6.3.6
    localCount = 0;
    for (size_t i = 0; i < count; i++) {
      if (results[i].sa.lrsDone) {
        entropyResults[localCount] = results[i].sa.lrsEntropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s LRS Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s LRS Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.7
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].mcw.done) {
        entropyResults[localCount] = results[i].mcw.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s MultiMCW Prediction Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s MultiMCW Prediction Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.8
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].lag.done) {
        entropyResults[localCount] = results[i].lag.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s Lag Prediction Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s Lag Prediction Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.9
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].mmc.done) {
        entropyResults[localCount] = results[i].mmc.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s MultiMMC Prediction Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s MultiMMC Prediction Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  // 6.3.10
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].lz78y.done) {
        entropyResults[localCount] = results[i].lz78y.entropy;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Assessment Bootstrap %s LZ78Y Prediction Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.5, entropyResults, localCount, 0.0, (double)bitWidth, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      fprintf(stderr, "Assessment Bootstrap %s LZ78Y Prediction Estimate: min entropy = %.17g\n", results[0].label, testRes);
      minminent = fmin(testRes, minminent);
    }
  }

  free(entropyResults);
  entropyResults = NULL;

  return minminent;
}

#define PREDICTORCOUNT(member)                                      \
  do {                                                              \
    for (size_t i = 0; i < count; i++) {                            \
      if (results[i].member.done) {                                 \
        entropyResults[localCount] = results[i].member.Pglobal;     \
        if (results[i].member.r > maxr) maxr = results[i].member.r; \
        localCount++;                                               \
        if (k == 0) {                                               \
          k = results[i].member.k;                                  \
          assert(k > 0);                                            \
        } else                                                      \
          assert(k == results[i].member.k);                         \
        if (N == 0) {                                               \
          N = results[i].member.N;                                  \
          assert(N > 0);                                            \
        } else                                                      \
          assert(N == results[i].member.N);                         \
      }                                                             \
    }                                                               \
  } while (0)

static double summerizePredictor(double *entropyResults, size_t localCount, size_t k, size_t N, size_t maxr, const char *testName, const char *label, struct randstate *rstate) {
  double PglobalBound;
  double Plocal;
  double entropy;
  double testRes;
  double runningMax;
  double confidenceInterval[2];

  assert(localCount > 0);
  assert(k > 0);
  assert(N > 0);
  assert(maxr > 0);

  runningMax = 1.0 / ((double)k);

  if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s %s Prediction Estimate: results = %zu\n", label, testName, localCount);
  testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
  PglobalBound = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
  if (PglobalBound < 0.0)
    PglobalBound = 0.0;
  else if (PglobalBound > 1.0)
    PglobalBound = 1.0;
  if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s %s Prediction Estimate: P_global' = %.17g\n", label, testName, PglobalBound);
  runningMax = fmax(runningMax, PglobalBound);

  Plocal = calcPlocal(N, maxr, k, runningMax, localCount, true);
  runningMax = fmax(runningMax, Plocal);

  if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s %s Prediction Estimate: P_local = %.17g\n", label, testName, Plocal);

  entropy = -log2(runningMax);
  fprintf(stderr, "Parameter Bootstrap %s %s Prediction Estimate: min entropy = %.17g\n", label, testName, entropy);
  return entropy;
}

double bootstrapParameters(struct entropyTestingResult *results, size_t count, size_t bitWidth, double *IIDminent, struct randstate *rstate) {
  double *entropyResults;
  double confidenceInterval[2];
  double minminent = DBL_INFINITY;

  assert(results != NULL);
  assert(count > 0);
  assert(bitWidth > 0);
  assert(bitWidth <= STATDATA_BITS);
  assert(rstate != NULL);
  assert(rstate->seeded);
  assert(count >= 200);

  if(IIDminent != NULL) *IIDminent = -1.0;

  if ((entropyResults = malloc(sizeof(double) * count)) == NULL) {
    perror("Can't allocate memory for result data");
    exit(EX_OSERR);
  }

  // 6.3.1
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].mcv.done) {
        entropyResults[localCount] = results[i].mcv.phat;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;
      double pu;
      double entropy;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Most Common Value Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      pu = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (pu < 0.0)
        pu = 0.0;
      else if (pu > 1.0)
        pu = 1.0;
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Most Common Value Estimate: p_u = %.17g\n", results[0].label, pu);
      entropy = -log2(pu);
      fprintf(stderr, "Parameter Bootstrap %s Most Common Value Estimate: min entropy = %.17g\n", results[0].label, entropy);
      minminent = fmin(entropy, minminent);

      if(IIDminent != NULL) *IIDminent = entropy;
    }
  }

  // 6.3.2
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].cols.done) {
        entropyResults[localCount] = results[i].cols.mean;
        localCount++;
      }
    }

    if (localCount > 0) {
      struct colsResult bootBound;
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Collision Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.005, entropyResults, localCount, 0.0, DBL_INFINITY, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.meanbound = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Collision Estimate: X-bar' = %.17g\n", results[0].label, bootBound.meanbound);
      // Calculate the result based on the provided X-bar'
      // L doesn't change the processing post-bounding.
      collisionEstimate(NULL, SIZE_MAX, &bootBound);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Collision Estimate: p = %.17g\n", results->label, bootBound.p);
      fprintf(stderr, "Parameter Bootstrap %s Collision Estimate: min entropy = %.17g\n", results->label, bootBound.entropy);
      minminent = fmin(bootBound.entropy, minminent);
    }
  }

  // 6.3.3
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].markov.done) {
        entropyResults[localCount] = results[i].markov.P0;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;
      struct markovResult bootBound;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.P0 = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_0 = %.17g\n", results[0].label, bootBound.P0);

      localCount = 0;
      for (size_t i = 0; i < count; i++) {
        if (results[i].markov.done) {
          entropyResults[localCount] = results[i].markov.P1;
          localCount++;
        }
      }
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.P1 = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_1 = %.17g\n", results[0].label, bootBound.P1);

      localCount = 0;
      for (size_t i = 0; i < count; i++) {
        if (results[i].markov.done) {
          entropyResults[localCount] = results[i].markov.T[0][0];
          localCount++;
        }
      }
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.T[0][0] = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_{0,0} = %.17g\n", results[0].label, bootBound.T[0][0]);

      localCount = 0;
      for (size_t i = 0; i < count; i++) {
        if (results[i].markov.done) {
          entropyResults[localCount] = results[i].markov.T[0][1];
          localCount++;
        }
      }
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.T[0][1] = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_{0,1} = %.17g\n", results[0].label, bootBound.T[0][1]);

      localCount = 0;
      for (size_t i = 0; i < count; i++) {
        if (results[i].markov.done) {
          entropyResults[localCount] = results[i].markov.T[1][0];
          localCount++;
        }
      }
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.T[1][0] = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_{1,0} = %.17g\n", results[0].label, bootBound.T[1][0]);

      localCount = 0;
      for (size_t i = 0; i < count; i++) {
        if (results[i].markov.done) {
          entropyResults[localCount] = results[i].markov.T[1][1];
          localCount++;
        }
      }
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.T[1][1] = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: P_{1,1} = %.17g\n", results[0].label, bootBound.T[1][1]);

      // Calculate the result based on the calculated parameter bounds
      // L doesn't change the processing post-bounding.
      markovEstimate(NULL, SIZE_MAX, &bootBound);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: p-hat_max = %.17g\n", results[0].label, bootBound.phatmax);
      fprintf(stderr, "Parameter Bootstrap %s Markov Estimate: min entropy = %.17g\n", results[0].label, bootBound.entropy);

      minminent = fmin(bootBound.entropy, minminent);
    }
  }

  // 6.3.4
  {
    size_t localCount = 0;
    size_t L = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].comp.done) {
        entropyResults[localCount] = results[i].comp.mean;
        localCount++;
        if (L == 0)
          L = results[i].comp.L;
        else
          assert(L == results[i].comp.L);
      }
    }

    if (localCount > 0) {
      struct compResult bootBound;
      double testRes;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Compression Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.005, entropyResults, localCount, 0.0, DBL_INFINITY, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      bootBound.meanbound = fmin(fmin(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Compression Estimate: X-bar' = %.17g\n", results[0].label, bootBound.meanbound);
      // Calculate the result based on the provided X-bar'
      compressionEstimate(NULL, L, &bootBound);
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s Compression Estimate: p = %.17g\n", results->label, bootBound.p);
      fprintf(stderr, "Parameter Bootstrap %s Compression Estimate: min entropy = %.17g\n", results->label, bootBound.entropy);
      minminent = fmin(bootBound.entropy, minminent);
    }
  }

  // 6.3.5 and 6.3.6
  {
    size_t localCount = 0;

    for (size_t i = 0; i < count; i++) {
      if (results[i].sa.tTupleDone) {
        entropyResults[localCount] = results[i].sa.tTuplePmax;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;
      double entropy;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s t-Tuple Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (testRes < 0.0) testRes = 0.0;
      if (testRes > 1.0) testRes = 1.0;
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s t-Tuple Estimate: p_u = %.17g\n", results[0].label, testRes);
      entropy = -log2(testRes);
      fprintf(stderr, "Parameter Bootstrap %s t-Tuple Estimate: min entropy = %.17g\n", results[0].label, entropy);
      minminent = fmin(entropy, minminent);
    }

    localCount = 0;
    for (size_t i = 0; i < count; i++) {
      if (results[i].sa.lrsDone) {
        entropyResults[localCount] = results[i].sa.lrsPmax;
        localCount++;
      }
    }

    if (localCount > 0) {
      double testRes;
      double entropy;

      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s LRS Estimate: results = %zu\n", results[0].label, localCount);
      testRes = BCaBootstrapPercentile(0.995, entropyResults, localCount, 0.0, 1.0, confidenceInterval, configBootstrapRounds, 0.99, rstate);
      testRes = fmax(fmax(testRes, confidenceInterval[0]), confidenceInterval[1]);
      if (testRes < 0.0)
        testRes = 0.0;
      else if (testRes > 1.0)
        testRes = 1.0;
      if (configVerbose > 0) fprintf(stderr, "Parameter Bootstrap %s LRS Estimate: p_u = %.17g\n", results[0].label, testRes);
      entropy = -log2(testRes);
      fprintf(stderr, "Parameter Bootstrap %s LRS Estimate: min entropy = %.17g\n", results[0].label, entropy);
      minminent = fmin(entropy, minminent);
    }
  }

  // 6.3.7 mcw MultiMCW
  {
    size_t localCount = 0;
    size_t k = 0;
    size_t N = 0;
    size_t maxr = 0;

    PREDICTORCOUNT(mcw);

    if (localCount > 0) {
      double entropy;
      entropy = summerizePredictor(entropyResults, localCount, k, N, maxr, "MultiMCW", results[0].label, rstate);
      minminent = fmin(entropy, minminent);
    }
  }

  // 6.3.8 lag Lag
  {
    size_t localCount = 0;
    size_t k = 0;
    size_t N = 0;
    size_t maxr = 0;

    PREDICTORCOUNT(lag);

    if (localCount > 0) {
      double entropy;
      entropy = summerizePredictor(entropyResults, localCount, k, N, maxr, "Lag", results[0].label, rstate);
      minminent = fmin(entropy, minminent);
    }
  }

  // 6.3.9 mmc MultiMMC
  {
    size_t localCount = 0;
    size_t k = 0;
    size_t N = 0;
    size_t maxr = 0;

    PREDICTORCOUNT(mmc);

    if (localCount > 0) {
      double entropy;
      entropy = summerizePredictor(entropyResults, localCount, k, N, maxr, "MultiMMC", results[0].label, rstate);
      minminent = fmin(entropy, minminent);
    }
  }

  // 6.3.10 lz78y LZ78Y
  {
    size_t localCount = 0;
    size_t k = 0;
    size_t N = 0;
    size_t maxr = 0;

    PREDICTORCOUNT(lz78y);

    if (localCount > 0) {
      double entropy;
      entropy = summerizePredictor(entropyResults, localCount, k, N, maxr, "LZ78Y", results[0].label, rstate);
      minminent = fmin(entropy, minminent);
    }
  }

  free(entropyResults);
  entropyResults = NULL;

  return minminent;
}
