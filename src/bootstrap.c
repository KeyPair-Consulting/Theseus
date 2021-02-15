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
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <omp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "bootstrap.h"
#include "cephes.h"
#include "fancymath.h"
#include "incbeta.h"
#include "randlib.h"

#define BIRTHDAYBOUNDEXP 10
// The binomial cutoff is the maximum probability that we can tolerate that there are not suitably extremal values.
// As a reference, if you want there to be:
//    * less than a 50% likelihood of there being 4 or fewer values above the 99.5 percentile, you need 934 data samples.
//    * less than a 5% likelihood of there being 4 or fewer values above the 99.5 percentile, you need 1829 data samples.
//    * less than a 1% likelihood of there being 4 or fewer values above the 99.5 percentile, you need 2318 data samples.
//    * less than a 0.01% likelihood of there being 4 or fewer values above the 99.5 percentile, you need 3550 data samples.
//    * less than a 50% likelihood of of there being 0 values above the 99.5 percentile, you need 139 data samples.
//    * less than a 5% likelihood of of there being 4 or fewer values above the 50th percentile, you need 21 data samples.
//    * less than a 1% likelihood of of there being 4 or fewer values above the 50th percentile, you need 24 data samples.
//    * less than a 0.01% likelihood of of there being 4 or fewer values above the 50th percentile, you need 28 data samples.
#define BINOMIALCUTOFF 0.5
#define SMALLEST_SIGNIFICANT 5LU
// Guidance for "at least 30 data elements" comes from Chernick's "Bootstrap Methods".
#define SMALLEST_BOOTSTRAP_SAMPLE 30LU

/*This is the a bootstrap re-sample*/
static void bootstrapSample(const double *data, double *bootstrapData, size_t datalen, struct randstate *rstate) {
  size_t i;

  assert(datalen > 0);
  assert(datalen - 1 <= UINT32_MAX);

  for (i = 0; i < datalen; i++) {
    bootstrapData[i] = data[randomRange((uint32_t)(datalen - 1), rstate)];
  }
}

/*Assume the data is sorted. Find the number of elements below the passed in value*/
size_t belowValue(double value, const double *data, size_t datalen) {
  size_t lowindex;
  size_t highindex;
  size_t midpoint;
  assert(data != NULL);
  assert(datalen > 0);

  if (data[0] >= value) {
    return 0;
  }
  if (data[datalen - 1] < value) {
    return datalen;
  }

  lowindex = 0;
  highindex = datalen - 1;
  assert(lowindex < highindex);

  // invariant: data[lowindex] < value
  // invariant: data[highindex] >= value
  // invariant: highindex < datalen
  // invariant: lowindex >= 0 (naturally enforced by types)
  while (lowindex < (highindex - 1)) {
    // lowindex < highindex true by the loop condition
    assert(highindex < datalen);
    assert(data[lowindex] < value);
    assert(data[highindex] >= value);
    // Midpoint calculation, trying to avoid integer rollover.
    midpoint = lowindex + (highindex - lowindex) / 2;
    assert(midpoint < highindex);
    assert(midpoint > lowindex);

    if (data[midpoint] < value) {
      assert(midpoint != lowindex);
      lowindex = midpoint;
    } else {
      assert(midpoint != highindex);
      highindex = midpoint;
    }
  }

  assert(lowindex == highindex - 1);
  // done with the search; verify invariants
  assert(highindex < datalen);
  assert(data[lowindex] < value);
  assert(data[highindex] >= value);

  // So, the array up to lowindex is strictly less than the reference
  // Ex: lowindex = 0, the results should be 1
  // Ex: lowindex = 2, the results should be 3

  // Now lowindex is the index of the first value that is strictly less than the reference value
  return lowindex + 1;
}

/*Assume the data is sorted. Find the number of elements above the passed in value*/
size_t aboveValue(double value, const double *data, size_t datalen) {
  size_t lowindex;
  size_t highindex;
  size_t midpoint;
  assert(data != NULL);
  assert(datalen > 0);

  if (data[0] > value) {
    return datalen;
  }
  if (data[datalen - 1] <= value) {
    return 0;
  }

  lowindex = 0;
  highindex = datalen - 1;
  assert(lowindex < highindex);

  // invariant: data[lowindex] <= value
  // invariant: data[highindex] > value
  // invariant: highindex < datalen
  // invariant: lowindex >= 0 (naturally enforced by types)
  while (lowindex < (highindex - 1)) {
    assert(lowindex < highindex);
    assert(highindex < datalen);
    assert(data[lowindex] <= value);
    assert(data[highindex] > value);
    // Midpoint calculation, trying to avoid integer rollover.
    midpoint = lowindex + (highindex - lowindex) / 2;
    assert(midpoint < highindex);
    assert(midpoint > lowindex);

    if (data[midpoint] <= value) {
      assert(midpoint != lowindex);
      lowindex = midpoint;
    } else {
      assert(midpoint != highindex);
      highindex = midpoint;
    }
  }

  assert(lowindex == highindex - 1);
  // done with the search; verify invariants
  assert(highindex < datalen);
  assert(data[highindex - 1] <= value);
  assert(data[highindex] > value);

  // Ex: highIndex = 3, datalen = 4, the desired result is 1.
  // Ex: highIndex = 1, datalen = 4, the desired result is 3.
  // Now lowindex is the index of the first value that is strictly less than the reference value
  return datalen - highindex;
}

static size_t valueCount(double *data, size_t datalen) {
  size_t count = 1;
  double curValue;

  // Deal with the empty list case
  if (datalen == 0) return 0;

  // Otherwise, we have at least one value.
  curValue = data[0];

  for (size_t j = 1; j < datalen; j++) {
    if (!relEpsilonEqual(curValue, data[j], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      count++;
      curValue = data[j];  // Updating this only when we find inequality maximizes the distinct count
    }
    // If we wanted to minimize the distinct count, we would update curValue here instead.
  }

  return count;
}

// data is assumed to be sorted
// We don't need to return theta here, as theta is just the sample mean (which we already know)
static void calculateJackknifeMeanEstimates(const double *data, double *JKEst, size_t datalen) {
  struct compensatedState totalDataSum;
  double dataSum;

  initCompensatedSum(&totalDataSum, "Mean Estimate Sum", 7);

  assert(data != NULL);
  assert(datalen > 1);

  for (size_t i = 0; i < datalen; i++) {
    compensatedSum(&totalDataSum, data[i]);
  }
  dataSum = compensatedSumResult(&totalDataSum);
  delCompensatedSum(&totalDataSum);

  for (size_t i = 0; i < datalen; i++) {
    // We are now dropping i
    JKEst[i] = (dataSum - data[i]) / ((double)(datalen - 1));
    if (configVerbose > 6) fprintf(stderr, "Mean Jackknife[%zu] = %.17g\n", i, JKEst[i]);
  }
}

#define JKINDEX(i, d) (((d) > (i)) ? (i) : ((i) + 1))

// percentile estimated using the recommended NIST method (Hyndman and Fan's R6).
// See: http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm
// data is assumed to be sorted
// Return the average estimate
static double calculateJackknifePercentileEstimates(double p, const double *data, double *JKEst, size_t datalen) {
  size_t k;
  size_t i;
  double kdouble;
  double d;
  struct compensatedState estimateSum;
  double endSum;

  initCompensatedSum(&estimateSum, "Estimate Sum", 4);

  assert((p >= 0) && (p <= 1));
  assert(data != NULL);
  assert(datalen > 1);

  d = modf(p * (double)(datalen), &kdouble);
  k = (size_t)kdouble;  // kdouble is integer

  for (i = 0; i < datalen; i++) {
    // We are now dropping i
    if (k == 0) {
      JKEst[i] = data[JKINDEX(0, i)];
    } else if (k >= datalen - 1) {
      JKEst[i] = data[(i != (datalen - 1)) ? (datalen - 1) : (datalen - 2)];
    } else {
      // The only values ever used here are data[k] and data[k-1] (in the re-written array indices)
      // Linear interpolation between the points
      JKEst[i] = data[JKINDEX(k - 1, i)] + d * (data[JKINDEX(k, i)] - data[JKINDEX(k - 1, i)]);
    }
    if (configVerbose > 6) fprintf(stderr, "Percentile Jackknife[%zu] = %.17g\n", i, JKEst[i]);
    compensatedSum(&estimateSum, JKEst[i]);
  }
  endSum = compensatedSumResult(&estimateSum);
  delCompensatedSum(&estimateSum);

  return endSum / (double)datalen;
}

double calculateMean(double *data, size_t datalen) {
  struct compensatedState totalDataSum;
  double dataSum;

  initCompensatedSum(&totalDataSum, "Mean Estimate Sum", 9);

  assert(data != NULL);
  assert(datalen > 0);

  for (size_t i = 0; i < datalen; i++) {
    compensatedSum(&totalDataSum, data[i]);
  }
  dataSum = compensatedSumResult(&totalDataSum);
  delCompensatedSum(&totalDataSum);
  return dataSum / ((double)datalen);
}

// percentile estimated using the recommended NIST method (Hyndman and Fan's R6).
// See: http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm
// Data is assumed to be in sorted order if sorted is set. If not, data is sorted as a side effect.
double processedCalculatePercentile(double p, double *data, size_t datalen, bool sorted, int extraVerbose) {
  size_t k;
  double kdouble;
  double d;
  double percentileValueCanidate;
  size_t numUnder, numOver, numEqual;

  assert((p >= 0) && (p <= 1));
  assert(data != NULL);
  assert(datalen > 0);

  if (!sorted) {
    // Now sort the data
    qsort(data, datalen, sizeof(double), doublecompare);
  }

  // The only values ever used here are data[k] and data[k-1]
  d = modf(p * (double)(datalen + 1), &kdouble);
  k = (size_t)kdouble;  // kdouble is integer

  assert((d >= 0.0) && (d < 1.0));

  if (k == 0) {
    percentileValueCanidate = data[0];
  } else if (k >= datalen) {
    percentileValueCanidate = data[datalen - 1];
  } else {
    // Linear interpolation between the points
    percentileValueCanidate = data[k - 1] + d * (data[k] - data[k - 1]);
  }

  if (configVerbose + extraVerbose > 1) {
    numUnder = belowValue(percentileValueCanidate, data, datalen);
    numOver = aboveValue(percentileValueCanidate, data, datalen);
    assert(numUnder + numOver <= datalen);
    numEqual = datalen - numUnder - numOver;

    fprintf(stderr, "Data set Percentile rank: %.17g %%\n", 100.0 * (((double)numUnder) + ((double)numEqual) * 0.5) / ((double)datalen));
  }

  return percentileValueCanidate;
}

// Assumes that data is sorted.
size_t trimDoubleRange(double **dataptr, size_t datalen, double validMin, double validMax) {
  double *revisedData;
  size_t transitionIndex;

  assert(dataptr != NULL);
  assert(*dataptr != NULL);

  if (datalen == 0) return 0LU;

  revisedData = *dataptr;

  // Trim off the invalid data.
  if (isfinite(validMin) && (transitionIndex = belowValue(validMin, revisedData, datalen)) > 0) {
    if (configVerbose > 1) fprintf(stderr, "%zu values under valid minimum discarded.\n", transitionIndex);
    revisedData += transitionIndex;
    datalen -= transitionIndex;
  }

  if ((datalen > 0) && isfinite(validMax) && ((transitionIndex = aboveValue(validMax, revisedData, datalen)) > 0)) {
    if (configVerbose > 1) fprintf(stderr, "%zu values above valid maximum discarded.\n", transitionIndex);
    datalen -= transitionIndex;
  }

  *dataptr = revisedData;
  return datalen;
}

double calculatePercentile(double p, double *data, size_t datalen, double validMin, double validMax) {
  double *trimmedData;

  // sort the data
  qsort(data, datalen, sizeof(double), doublecompare);
  trimmedData = data;
  datalen = trimDoubleRange(&trimmedData, datalen, validMin, validMax);
  if (configVerbose > 1) fprintf(stderr, "Sample is %zu valid samples.\n", datalen);
  return processedCalculatePercentile(p, trimmedData, datalen, true, 0);
}

/*Return the percentile.
Puts the derived confidence interval into confidenceInterval.*/
/*This essentially follows Efron and Tibshirani (chapter 14), with some corrections from Efron and Hastie's "Computer Age Statistical Inference" (Section 11.3);
 *all the implementations that I could track down use this approach. This is also the approach present in Helwig's excellent lecture notes:
 *http://users.stat.umn.edu/~helwig/notes/bootci-Notes.pdf
 *Note, Efron and Tibsirani is outvoted on the question of if "equal" should be included when calculating z0 (Efron-Hastie and Davison-Hinkley both include equality).
 *The consensus seems to use the bootstrap distribution function (which thus includes equality).
 *The Davison-Hinkley's book divides this by the number of rounds plus one. The reasoning for this choice is opaque to me; it has the feel of a variance bias fix.
 *Efron-Tibshirani (with the exception of including equality when calculating z0) and Efron-Hastie proceed as we do.
 *Davison-Hinkley's book discuss finding 'a' with respect to some sort of partial derivative; for non-parametric
 *data, this is taken to be partials of the linear interpolation induced by the data. It would seem that this is
 *unreasonably fancy! (partials of linear interpolations aren't interesting!)
 *
 *For this code, if the requested percentile is not possible to assess (as no data that extreme is likely to be present) then we return the min/max
 *(whichever corresponds to the requested percentile). No confidence intervals are generated in this case (and no bootstrapping occurs).
 *If
 *    there are expected to be fewer than SMALLEST_SIGNIFICANT elements more extreme than the requested percentile, OR
 *    there are fewer than SMALLEST_BOOTSTRAP_SAMPLE samples, OR
 *    the number of samples makes repeated selections likely,
 * then a bootstrap is calculated, but the extremal values are returned.
 * else
 *If 'a' is undefined or 0, there is no BCa interval, and we instead produce a BC percentile interval.
 *If z0 is infinite (for example, if all bootstraps are effectively the same), then we use a percentile bootstrap.
 *If the produced confidence interval does not include the observed percentile, then fall back to a percentile bootstrap.
 */
double BCaBootstrapPercentile(double p, double *data, size_t datalen, double validMin, double validMax, double *confidenceInterval, size_t rounds, double alpha, struct randstate *rstate) {
  size_t i;
  double *bootstrapPercentiles;
  double percentile;
  struct compensatedState runningAccelNumerator;
  struct compensatedState runningAccelDenominator;
  long double accelNumerator;
  long double accelDenominator;

  double z0;  // bias
  double zCur;  // bias
  double a;  // acceleration factor
  double alphaPrime;  // acceleration factor
  double alpha1;
  double alpha2;
  size_t valuesUnderPercentile;
  bool validBias;
  bool validAcceleration;
  bool useExtremalBootstrapValues = false;
  double diversity;
  size_t numOfBootstrapValues;
  size_t numOfDataValues;

  int exceptions;
  uint64_t selectionsNeeded;
  double *trimmedData;

  assert(data != NULL);
  assert(confidenceInterval != NULL);
  assert(rounds > 0);
  assert((p >= 0.0) && (p <= 1.0));
  assert((alpha >= 0.0) && (alpha <= 1.0));
  assert(rstate != NULL);
  assert(datalen > 0);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  // It's a two-sided interval.
  // For example, for a 99% confidence interval, alpha is 0.99.
  // Corresponding alphaPrime would be 0.005
  alphaPrime = (1.0 - alpha) / 2.0;

  qsort(data, datalen, sizeof(double), doublecompare);
  // The data is now sorted.

  // Trim off the invalid data
  trimmedData = data;
  datalen = trimDoubleRange(&trimmedData, datalen, validMin, validMax);
  if (configVerbose > 1) fprintf(stderr, "Sample is %zu valid samples.\n", datalen);

  if (datalen == 0) {
    // The data is all invalid.
    // Return a (presumably invalid) data item as a flag.
    fprintf(stderr, "Data set contains no valid data. Returning out of bounds values.\n");
    confidenceInterval[0] = data[0];
    confidenceInterval[1] = data[0];
    return data[0];
  } else if (datalen == 1) {
    // We have exactly 1 valid data item.
    fprintf(stderr, "Data set contains only 1 element. All percentiles are equal to this one element. Returning this value.\n");
    confidenceInterval[0] = trimmedData[0];
    confidenceInterval[1] = trimmedData[0];
    return trimmedData[0];
  }

  // JEH: Could use sub-sampling to get an estimate in this case
  // We care about two basic values here:
  //    * do we have at least a 1-BINOMIALCUTOFF likelihood of seeing at least 1 value more extreme than the requested proportion?
  //    * do we have at least a 1-BINOMIALCUTOFF likelihood of seeing at least SMALLEST_SIGNIFICANT value more extreme than the requested proportion?
  // We use the binomial distribution to determine if this is so.
  {
    double moreExtremeProp = fmin(p, 1.0 - p);
    double bound;

    // bound is the probability of fewer than SMALLEST_SIGNIFICANT extremal elements
    bound = binomialCDF(SMALLEST_SIGNIFICANT - 1, datalen, moreExtremeProp);
    
    if(fetestexcept(FE_UNDERFLOW) != 0) {
      if(configVerbose > 0) fprintf(stderr, "Clearing expected binomial CDF underflow when checking if data is likely meaningful.\n");
      if(bound <= DBL_MIN) bound = 0.0;
      feclearexcept(FE_UNDERFLOW);
    } 

    if(configVerbose > 1) {
      if(bound < DBL_MIN) {
        fprintf(stderr, "There is essentially no chance that this data will not include suitable extremal values.\n");
      } else {
        fprintf(stderr, "Probability of there being fewer than %zu samples more extreme than the sought percentile in a set of %zu samples = %.17g\n", SMALLEST_SIGNIFICANT, datalen, bound);
      }
    }

    if (bound > BINOMIALCUTOFF) {
      // We don't have enough for the normal process... Check to see if the percentile likely makes sense...
      // bound is the probability of there being 0 extremal elements
      bound = binomialCDF(0LU, datalen, moreExtremeProp);
      if(fetestexcept(FE_UNDERFLOW) != 0) {
        if(configVerbose > 0) fprintf(stderr, "Clearing expected binomial CDF underflow when checking if data could be meaningful.\n");
        if(bound <= DBL_MIN) bound = 0.0;
        feclearexcept(FE_UNDERFLOW);
      } 

      if (configVerbose > 1) fprintf(stderr, "Probability of there being no samples more extreme than the sought percentile in a set of %zu samples = %.17g\n", datalen, bound);

      if (bound > BINOMIALCUTOFF) {
        // We don't have enough for any meaningful process
        fprintf(stderr, "There is a significant chance that the data set doesn't contain any data more extremal than the requested percentile. Returning only the extremal data value.\n");
        if (p >= 0.5) {
          // Return the max of the data set
          confidenceInterval[0] = confidenceInterval[1] = percentile = data[datalen - 1];
        } else {
          // Return the min of the data set
          confidenceInterval[0] = confidenceInterval[1] = percentile = data[0];
        }
        if (configVerbose > 0) fprintf(stderr, "Sample %.17g %% Percentile: %.17g, Data Extremal Value: [ %.17g, %.17g ]\n", p * 100.0, percentile, confidenceInterval[0], confidenceInterval[1]);
        return percentile;
      } else {
        // But we can at least look at a bootstrap distribution.
        if (configVerbose > 1) fprintf(stderr, "An extremal value is being sought. Returning extremal bootstrap values as confidence interval.\n");
        useExtremalBootstrapValues = true;
      }
    }
    assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  }

  // data is now sorted and all within a valid range.
  percentile = processedCalculatePercentile(p, data, datalen, true, 0);
  if (configVerbose > 1) fprintf(stderr, "Sample %.17g %% Percentile: %.17g\n", p * 100.0, percentile);

  // Count the number of distinct data values.
  numOfDataValues = valueCount(data, datalen);
  if (configVerbose > 1) {
    fprintf(stderr, "Data has %zu distinct values.\n", numOfDataValues);
  }

  // The number of ways of selecting k elements from an n-set with replacement is C(n+k-1, k).
  // We want to make sure that there are enough possible symbol index selections so that the selections aren't
  // likely be forced to repeat across bootstraps.
  // We use a goal a probability of less than 2^-BIRTHDAYBOUNDEXP bound for birthday collision.
  selectionsNeeded = selectionsForBirthdayCollisionBound(rounds, BIRTHDAYBOUNDEXP);
  if (configVerbose > 3) fprintf(stderr, "We are targeting more than %zu selections\n", selectionsNeeded);

  if (!useExtremalBootstrapValues && (datalen < SMALLEST_BOOTSTRAP_SAMPLE)) {
    fprintf(stderr, "There is too little data. Returning extremal bootstrap values.\n");
    useExtremalBootstrapValues = true;
  } else if (!useExtremalBootstrapValues && !combinationsGreaterThanBound((datalen << 1) - 1, datalen, selectionsNeeded)) {  // datalen + datalen - 1 is the same as (datalen<<1) - 1
    fprintf(stderr, "The Data has insufficient distinct values to support meaningful a bootstrap. Returning extremal bootstrap values.\n");
    useExtremalBootstrapValues = true;
  }

  if ((bootstrapPercentiles = malloc(sizeof(double) * rounds)) == NULL) {
    perror("Can't allocate room for bootstrap percentiles");
    exit(EX_OSERR);
  }

#pragma omp parallel
  {
    struct randstate threadRstate;
    double *bootstrapData;

    initGenerator(&threadRstate);
    seedGenerator(&threadRstate);

    // Do the bootstrap sampling
    if ((bootstrapData = malloc(sizeof(double) * datalen)) == NULL) {
      perror("Can't allocate room for bootstrap");
      exit(EX_OSERR);
    }
#pragma omp for
    for (size_t j = 0; j < rounds; j++) {
      bootstrapSample(data, bootstrapData, datalen, &threadRstate);
      bootstrapPercentiles[j] = processedCalculatePercentile(p, bootstrapData, datalen, false, -8);
      if (configVerbose > 6) fprintf(stderr, "Bootstrap percentile: %.17g\n", bootstrapPercentiles[j]);
    }
    free(bootstrapData);
  }  // end threads

  // Sort the resulting percentiles
  qsort(bootstrapPercentiles, rounds, sizeof(double), doublecompare);

  // Check to see if all the results are the same...
  if (relEpsilonEqual(bootstrapPercentiles[0], bootstrapPercentiles[rounds - 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    if (configVerbose > 0) fprintf(stderr, "All bootstrap values are the same.\n");
    assert(relEpsilonEqual(bootstrapPercentiles[0], percentile, ABSEPSILON, RELEPSILON, ULPEPSILON));
    confidenceInterval[0] = percentile;
    confidenceInterval[1] = percentile;
    free(bootstrapPercentiles);
    if (configVerbose > 0) fprintf(stderr, "Sample %.17g %% Percentile: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, rounds, confidenceInterval[0], confidenceInterval[1]);
    return percentile;
  }

  // Count the number of bootstrap values.
  numOfBootstrapValues = valueCount(bootstrapPercentiles, rounds);
  diversity = ((double)numOfBootstrapValues) / ((double)rounds);
  if (configVerbose > 1) {
    fprintf(stderr, "Bootstrap has %zu distinct values (proportion: %.17g)\n", numOfBootstrapValues, diversity);
  }

  if (useExtremalBootstrapValues) {
    confidenceInterval[0] = bootstrapPercentiles[0];
    confidenceInterval[1] = bootstrapPercentiles[rounds - 1];
    free(bootstrapPercentiles);
    if (configVerbose > 0) fprintf(stderr, "Sample %.17g %% Percentile: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, rounds, confidenceInterval[0], confidenceInterval[1]);
    return percentile;
  }

  // At this stage of the calculation, we haven't returned, so we expect that some sort of bootstrap confidence interval can be produced.

  // Now calculate the bias (based on the bootstrap CDF, thus including equality)
  valuesUnderPercentile = rounds - aboveValue(percentile, bootstrapPercentiles, rounds);

  if ((valuesUnderPercentile == 0) || (valuesUnderPercentile == rounds)) {
    if (configVerbose > 1) fprintf(stderr, "No or all values under reference value, so the bias is infinite. Producing a Bootstrap Percentile Confidence Interval.\n");
    validBias = false;
    z0 = DBL_INFINITY;
  } else {
    double rawBias;

    rawBias = ((double)valuesUnderPercentile) / ((double)rounds);
    validBias = true;
    z0 = cephes_ndtri(rawBias);
    if (configVerbose > 1) {
      fprintf(stderr, "Raw Bias: %.17g\n", rawBias);
      fprintf(stderr, "Bias: %.17g\n", z0);
    }
  }

  if (validBias) {
    bool validIntervals = true;
    double *JKests;
    double JKtheta;

    if ((JKests = malloc(sizeof(double) * datalen)) == NULL) {
      perror("Can't allocate room for JK estimates.");
      exit(EX_OSERR);
    }

    // Use a jackknife approach to estimate the acceleration factor for the original data
    JKtheta = calculateJackknifePercentileEstimates(p, data, JKests, datalen);
    if (configVerbose > 1) fprintf(stderr, "Jackknife theta: %.17g\n", JKtheta);

    initCompensatedSum(&runningAccelNumerator, "Accel Numerator Sum", 5);
    initCompensatedSum(&runningAccelDenominator, "Accel Denominator Sum", 6);
    assert(isfinite(JKtheta));
    for (i = 0; i < datalen; i++) {
      long double termsq, localterm;

      assert(isfinite(JKests[i]));
      localterm = (long double)(JKests[i] - JKtheta);
      termsq = localterm * localterm;
      if (configVerbose > 6) fprintf(stderr, "Jackknife delta^2: %.21Lg\n", termsq);
      compensatedSuml(&runningAccelNumerator, termsq * localterm);
      compensatedSuml(&runningAccelDenominator, termsq);
    }

    accelDenominator = compensatedSumResultl(&runningAccelDenominator);
    accelDenominator = 6.0L * powl(accelDenominator, 1.5L);
    assert(accelDenominator >= 0.0L);  // This is a sum of squares
    accelNumerator = compensatedSumResultl(&runningAccelNumerator);

    delCompensatedSum(&runningAccelDenominator);
    delCompensatedSum(&runningAccelNumerator);

    if (relEpsilonEqual((double)accelDenominator, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      if (configVerbose > 1) fprintf(stderr, "Acceleration Denominator is effectively zero. Producing a BC Bootstrap Percentile Confidence Interval.\n");
      validAcceleration = false;
      a = 0.0;
    } else {
      assert(accelDenominator > 0.0L);
      a = (double)(accelNumerator / accelDenominator);
      if (relEpsilonEqual(a, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        if (configVerbose > 1) fprintf(stderr, "Acceleration is effectively zero. Producing a BC Bootstrap Percentile Confidence Interval.\n");
        validAcceleration = false;
        a = 0.0;
      } else {
        validAcceleration = true;
        if (configVerbose > 1) fprintf(stderr, "Acceleration: %.17g\n", a);
      }
    }

    zCur = cephes_ndtri(alphaPrime);
    alpha1 = normalCDF(z0 + (z0 + zCur) / (1.0 - a * (z0 + zCur)), 0.0, 1.0);
    zCur = cephes_ndtri(1.0 - alphaPrime);
    alpha2 = normalCDF(z0 + (z0 + zCur) / (1.0 - a * (z0 + zCur)), 0.0, 1.0);

    assert(alpha1 <= alpha2);

    // Get the relevant values
    confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapPercentiles, rounds, true, -2);
    confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapPercentiles, rounds, true, -2);

    if ((percentile < confidenceInterval[0]) || (percentile > confidenceInterval[1])) {
      if (configVerbose > 0) fprintf(stderr, "Bias corrected confidence interval does not contain the observed percentile. Falling back to Percentile method.\n");
      validIntervals = false;
      alpha1 = alphaPrime;
      alpha2 = 1.0 - alphaPrime;
      assert(alpha1 <= alpha2);
      confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapPercentiles, rounds, true, -2);
      confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapPercentiles, rounds, true, -2);
      if ((percentile < confidenceInterval[0]) || (percentile > confidenceInterval[1])) {
        alpha1 = 0.0;
        alpha2 = 1.0;
        useExtremalBootstrapValues = true;
        fprintf(stderr, "Warning: Percentile Confidence interval does not contain the observed percentile. Returning extremal bootstrap values.\n");
        confidenceInterval[0] = bootstrapPercentiles[0];
        confidenceInterval[1] = bootstrapPercentiles[rounds - 1];
      }
    }

    if (configVerbose > 1) {
      fprintf(stderr, "alpha1: %.17g, alpha2: %.17g\n", alpha1, alpha2);
    }

    if (configVerbose > 0) {
      if (useExtremalBootstrapValues) {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else if (!validIntervals) {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, %.17g %% Percentile Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else if (validAcceleration) {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, %.17g %% BCa Percentile Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, %.17g %% BC Percentile Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      }
    }
    free(JKests);
  } else {
    // The BCa / BC bootstrap parameters aren't valid, so return a percentile bound.
    alpha1 = alphaPrime;
    alpha2 = 1.0 - alphaPrime;
    assert(alpha1 <= alpha2);

    confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapPercentiles, rounds, true, -2);
    confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapPercentiles, rounds, true, -2);
    if ((percentile < confidenceInterval[0]) || (percentile > confidenceInterval[1])) {
      useExtremalBootstrapValues = true;
      fprintf(stderr, "Warning: Percentile Confidence interval does not contain the observed percentile. Returning extremal bootstrap values.\n");
      confidenceInterval[0] = bootstrapPercentiles[0];
      confidenceInterval[1] = bootstrapPercentiles[rounds - 1];
      alpha1 = 0.0;
      alpha2 = 1.0;
    }

    if (configVerbose > 1) {
      fprintf(stderr, "alpha1: %.17g, alpha2: %.17g\n", alpha1, alpha2);
    }

    if (configVerbose > 0) {
      if (useExtremalBootstrapValues) {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else {
        fprintf(stderr, "Sample %.17g %% Percentile: %.17g, %.17g %% Percentile Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", p * 100.0, percentile, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      }
    }
  }

  assert(confidenceInterval[0] <= confidenceInterval[1]);

  free(bootstrapPercentiles);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions != 0) {
    fprintf(stderr, "Math error in bootstrap code: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  return percentile;
}

/*Return the mean.
Puts the derived confidence interval into confidenceInterval.*/
/*This essentially follows Efron and Tibshirani (chapter 14), with some corrections from Efron and Hastie's "Computer Age Statistical Inference" (Section 11.3);
 *all the implementations that I could track down use this approach. This is also the approach present in Helwig's excellent lecture notes:
 *http://users.stat.umn.edu/~helwig/notes/bootci-Notes.pdf
 *Note, Efron and Tibsirani is outvoted on the question of if "equal" should be included when calculating z0 (Efron-Hastie and Davison-Hinkley both include equality).
 *The consensus seems to use the bootstrap distribution function (which thus includes equality).
 *The Davison-Hinkley's book divides this by the number of rounds plus one. The reasoning for this choice is opaque to me; it has the feel of a variance bias fix.
 *Efron-Tibshirani (with the exception of including equality when calculating z0) and Efron-Hastie proceed as we do.
 *Davison-Hinkley's book discuss finding 'a' with respect to some sort of partial derivative; for non-parametric
 *data, this is taken to be partials of the linear interpolation induced by the data. It would seem that this is
 *unreasonably fancy! (partials of linear interpolations aren't interesting!)
 *
 *If 'a' is undefined or 0, there is no BCa interval, and we instead produce a BC mean interval.
 *If z0 is infinite (for example, if all bootstraps are effectively the same), then we use a percentile bootstrap.
 *If the produced confidence interval does not include the observed mean, then fall back to a percentile bootstrap.
 */
double BCaBootstrapMean(double *data, size_t datalen, double validMin, double validMax, double *confidenceInterval, size_t rounds, double alpha, struct randstate *rstate) {
  size_t i;
  double *bootstrapMeans;
  double sampleMean;
  struct compensatedState runningAccelNumerator;
  struct compensatedState runningAccelDenominator;
  long double accelNumerator;
  long double accelDenominator;

  double z0;  // bias
  double zCur;  // bias
  double a;  // acceleration factor
  double alphaPrime;  // acceleration factor
  double alpha1;
  double alpha2;
  size_t valuesUnderMean;
  bool validBias;
  bool validAcceleration;
  bool useExtremalBootstrapValues = false;
  double diversity;
  size_t numOfBootstrapValues;
  size_t numOfDataValues;
  double *trimmedData;

  int exceptions;
  uint64_t selectionsNeeded;

  assert(data != NULL);
  assert(confidenceInterval != NULL);
  assert(rounds > 0);
  assert((alpha >= 0.0) && (alpha <= 1.0));
  assert(rstate != NULL);
  assert(datalen > 0);
  assert(validMin <= validMax);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  // It's a two-sided interval.
  // For example, for a 99% confidence interval, alpha is 0.99.
  // Corresponding alphaPrime would be 0.005
  alphaPrime = (1.0 - alpha) / 2.0;

  qsort(data, datalen, sizeof(double), doublecompare);
  // The data is now sorted.

  // Trim off the invalid data
  trimmedData = data;
  datalen = trimDoubleRange(&trimmedData, datalen, validMin, validMax);
  if (configVerbose > 1) fprintf(stderr, "Sample is %zu valid samples.\n", datalen);

  if (datalen == 0) {
    // The data is all invalid.
    // Return a (presumably invalid) data item as a flag.
    fprintf(stderr, "Data set contains no valid data. Returning out of bounds values.\n");
    confidenceInterval[0] = data[0];
    confidenceInterval[1] = data[0];
    return data[0];
  } else if (datalen == 1) {
    // We have exactly 1 valid data item.
    fprintf(stderr, "Data set contains only 1 element. All means are equal to this one element. Returning this value.\n");
    confidenceInterval[0] = data[0];
    confidenceInterval[1] = data[0];
    return data[0];
  }

  // data is now sorted and all within a valid range.
  sampleMean = calculateMean(data, datalen);
  assert(isfinite(sampleMean));
  if (configVerbose > 1) fprintf(stderr, "Sample Mean: %.17g\n", sampleMean);

  // Count the number of distinct data values.
  numOfDataValues = valueCount(data, datalen);
  if (configVerbose > 1) {
    fprintf(stderr, "Data has %zu distinct values.\n", numOfDataValues);
  }

  // The number of ways of selecting k elements from an n-set with replacement is C(n+k-1, k).
  // We want to make sure that there are enough possible symbol index selections so that the selections aren't
  // likely be forced to repeat across bootstraps.
  // We use a goal a probability of less than 2^-BIRTHDAYBOUNDEXP bound for birthday collision.
  selectionsNeeded = selectionsForBirthdayCollisionBound(rounds, BIRTHDAYBOUNDEXP);
  if (configVerbose > 3) fprintf(stderr, "We are targeting more than %zu selections\n", selectionsNeeded);

  if (!useExtremalBootstrapValues && (datalen < SMALLEST_BOOTSTRAP_SAMPLE)) {
    fprintf(stderr, "There is too little data. Returning extremal bootstrap values.\n");
    useExtremalBootstrapValues = true;
  } else if (!useExtremalBootstrapValues && !combinationsGreaterThanBound((datalen << 1) - 1, datalen, selectionsNeeded)) {  // datalen + datalen - 1 is the same as (datalen<<1) - 1
    fprintf(stderr, "The Data has insufficient distinct values to support meaningful a bootstrap. Returning extremal bootstrap values.\n");
    useExtremalBootstrapValues = true;
  }

  if ((bootstrapMeans = malloc(sizeof(double) * rounds)) == NULL) {
    perror("Can't allocate room for bootstrap sample means");
    exit(EX_OSERR);
  }

#pragma omp parallel
  {
    struct randstate threadRstate;
    double *bootstrapData;

    initGenerator(&threadRstate);
    seedGenerator(&threadRstate);

    // Do the bootstrap sampling
    if ((bootstrapData = malloc(sizeof(double) * datalen)) == NULL) {
      perror("Can't allocate room for bootstrap");
      exit(EX_OSERR);
    }
#pragma omp for
    for (size_t j = 0; j < rounds; j++) {
      bootstrapSample(data, bootstrapData, datalen, &threadRstate);
      bootstrapMeans[j] = calculateMean(bootstrapData, datalen);
      if (configVerbose > 6) fprintf(stderr, "Bootstrap mean: %.17g\n", bootstrapMeans[j]);
    }
    free(bootstrapData);
  }  // end threads

  // Sort the resulting means
  qsort(bootstrapMeans, rounds, sizeof(double), doublecompare);

  // Check to see if all the results are the same...
  if (relEpsilonEqual(bootstrapMeans[0], bootstrapMeans[rounds - 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    if (configVerbose > 0) fprintf(stderr, "All bootstrap values are the same.\n");
    assert(relEpsilonEqual(bootstrapMeans[0], sampleMean, ABSEPSILON, RELEPSILON, ULPEPSILON));
    confidenceInterval[0] = sampleMean;
    confidenceInterval[1] = sampleMean;
    free(bootstrapMeans);
    if (configVerbose > 0) fprintf(stderr, "Sample Mean: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, rounds, confidenceInterval[0], confidenceInterval[1]);
    return sampleMean;
  }

  // Count the number of bootstrap values.
  numOfBootstrapValues = valueCount(bootstrapMeans, rounds);
  diversity = ((double)numOfBootstrapValues) / ((double)rounds);
  if (configVerbose > 1) {
    fprintf(stderr, "Bootstrap has %zu distinct values (proportion: %.17g)\n", numOfBootstrapValues, diversity);
  }

  if (useExtremalBootstrapValues) {
    confidenceInterval[0] = bootstrapMeans[0];
    confidenceInterval[1] = bootstrapMeans[rounds - 1];
    free(bootstrapMeans);
    if (configVerbose > 0) fprintf(stderr, "Sample Mean: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, rounds, confidenceInterval[0], confidenceInterval[1]);
    return sampleMean;
  }

  // At this stage of the calculation, we haven't returned, so we expect that some sort of bootstrap confidence interval can be produced.

  // Now calculate the bias (based on the bootstrap CDF, thus including equality)
  valuesUnderMean = rounds - aboveValue(sampleMean, bootstrapMeans, rounds);

  if ((valuesUnderMean == 0) || (valuesUnderMean == rounds)) {
    if (configVerbose > 1) fprintf(stderr, "No or all values under reference value, so the bias is infinite. Producing a Bootstrap Percentile Confidence Interval.\n");
    validBias = false;
    z0 = DBL_INFINITY;
  } else {
    double rawBias;

    rawBias = ((double)valuesUnderMean) / ((double)rounds);
    validBias = true;
    z0 = cephes_ndtri(rawBias);
    if (configVerbose > 1) {
      fprintf(stderr, "Raw Bias: %.17g\n", rawBias);
      fprintf(stderr, "Bias: %.17g\n", z0);
    }
  }

  if (validBias) {
    bool validIntervals = true;
    double *JKests;

    if ((JKests = malloc(sizeof(double) * datalen)) == NULL) {
      perror("Can't allocate room for JK estimates.");
      exit(EX_OSERR);
    }

    // Use a jackknife approach to estimate the acceleration factor for the original data
    calculateJackknifeMeanEstimates(data, JKests, datalen);
    // Note, for the mean, JKtheta is just the sample mean.

    initCompensatedSum(&runningAccelNumerator, "Accel Numerator Sum", 5);
    initCompensatedSum(&runningAccelDenominator, "Accel Denominator Sum", 6);
    for (i = 0; i < datalen; i++) {
      long double termsq, localterm;

      assert(isfinite(JKests[i]));
      localterm = (long double)(JKests[i] - sampleMean);
      termsq = localterm * localterm;
      if (configVerbose > 6) fprintf(stderr, "Jackknife delta^2: %.21Lg\n", termsq);
      compensatedSuml(&runningAccelNumerator, termsq * localterm);
      compensatedSuml(&runningAccelDenominator, termsq);
    }

    accelDenominator = compensatedSumResultl(&runningAccelDenominator);
    accelDenominator = 6.0L * powl(accelDenominator, 1.5L);
    assert(accelDenominator >= 0.0L);  // This is a sum of squares
    accelNumerator = compensatedSumResultl(&runningAccelNumerator);

    delCompensatedSum(&runningAccelDenominator);
    delCompensatedSum(&runningAccelNumerator);

    if (relEpsilonEqual((double)accelDenominator, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      if (configVerbose > 1) fprintf(stderr, "Acceleration Denominator is effectively zero. Producing a BC Bootstrap Percentile Confidence Interval.\n");
      validAcceleration = false;
      a = 0.0;
    } else {
      assert(accelDenominator > 0.0L);
      a = (double)(accelNumerator / accelDenominator);
      if (relEpsilonEqual(a, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        if (configVerbose > 1) fprintf(stderr, "Acceleration is effectively zero. Producing a BC Bootstrap Percentile Confidence Interval.\n");
        validAcceleration = false;
        a = 0.0;
      } else {
        validAcceleration = true;
        if (configVerbose > 1) fprintf(stderr, "Acceleration: %.17g\n", a);
      }
    }

    zCur = cephes_ndtri(alphaPrime);
    alpha1 = normalCDF(z0 + (z0 + zCur) / (1.0 - a * (z0 + zCur)), 0.0, 1.0);
    zCur = cephes_ndtri(1.0 - alphaPrime);
    alpha2 = normalCDF(z0 + (z0 + zCur) / (1.0 - a * (z0 + zCur)), 0.0, 1.0);

    assert(alpha1 <= alpha2);

    // Get the relevant values
    confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapMeans, rounds, true, -2);
    confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapMeans, rounds, true, -2);

    if ((sampleMean < confidenceInterval[0]) || (sampleMean > confidenceInterval[1])) {
      if (configVerbose > 0) fprintf(stderr, "Bias corrected confidence interval does not contain the observed sampleMean. Falling back to Percentile method.\n");
      validIntervals = false;
      alpha1 = alphaPrime;
      alpha2 = 1.0 - alphaPrime;
      assert(alpha1 <= alpha2);
      confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapMeans, rounds, true, -2);
      confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapMeans, rounds, true, -2);
      if ((sampleMean < confidenceInterval[0]) || (sampleMean > confidenceInterval[1])) {
        alpha1 = 0.0;
        alpha2 = 1.0;
        useExtremalBootstrapValues = true;
        fprintf(stderr, "Warning: Mean Confidence interval does not contain the observed sampleMean. Returning extremal bootstrap values.\n");
        confidenceInterval[0] = bootstrapMeans[0];
        confidenceInterval[1] = bootstrapMeans[rounds - 1];
      }
    }

    if (configVerbose > 1) {
      fprintf(stderr, "alpha1: %.17g, alpha2: %.17g\n", alpha1, alpha2);
    }

    if (configVerbose > 0) {
      if (useExtremalBootstrapValues) {
        fprintf(stderr, "Sample Mean: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else if (!validIntervals) {
        fprintf(stderr, "Sample Mean: %.17g, %.17g %% Mean Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else if (validAcceleration) {
        fprintf(stderr, "Sample Mean: %.17g, %.17g %% BCa Mean Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else {
        fprintf(stderr, "Sample Mean: %.17g, %.17g %% BC Mean Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      }
    }
    free(JKests);
  } else {
    // The BCa / BC bootstrap parameters aren't valid, so return a mean bound.
    alpha1 = alphaPrime;
    alpha2 = 1.0 - alphaPrime;
    assert(alpha1 <= alpha2);

    confidenceInterval[0] = processedCalculatePercentile(alpha1, bootstrapMeans, rounds, true, -2);
    confidenceInterval[1] = processedCalculatePercentile(alpha2, bootstrapMeans, rounds, true, -2);
    if ((sampleMean < confidenceInterval[0]) || (sampleMean > confidenceInterval[1])) {
      useExtremalBootstrapValues = true;
      fprintf(stderr, "Warning: Mean Confidence interval does not contain the observed sampleMean. Returning extremal bootstrap values.\n");
      confidenceInterval[0] = bootstrapMeans[0];
      confidenceInterval[1] = bootstrapMeans[rounds - 1];
      alpha1 = 0.0;
      alpha2 = 1.0;
    }

    if (configVerbose > 1) {
      fprintf(stderr, "alpha1: %.17g, alpha2: %.17g\n", alpha1, alpha2);
    }

    if (configVerbose > 0) {
      if (useExtremalBootstrapValues) {
        fprintf(stderr, "Sample Mean: %.17g, Bootstrap Extremal Values (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, rounds, confidenceInterval[0], confidenceInterval[1]);
      } else {
        fprintf(stderr, "Sample Mean: %.17g, %.17g %% Mean Bootstrap Confidence Interval (%zu bootstrap rounds): [ %.17g, %.17g ]\n", sampleMean, 100.0 * alpha, rounds, confidenceInterval[0], confidenceInterval[1]);
      }
    }
  }

  assert(confidenceInterval[0] <= confidenceInterval[1]);

  free(bootstrapMeans);

  exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
  if (exceptions != 0) {
    fprintf(stderr, "Math error in bootstrap code: ");
    if (exceptions & FE_INVALID) {
      fprintf(stderr, "FE_INVALID ");
    }
    if (exceptions & FE_DIVBYZERO) {
      fprintf(stderr, "Divided by 0 ");
    }
    if (exceptions & FE_OVERFLOW) {
      fprintf(stderr, "Found an overflow ");
    }
    if (exceptions & FE_UNDERFLOW) {
      fprintf(stderr, "Found an Underflow");
    }
    fprintf(stderr, "\n");
    exit(EX_DATAERR);
  }

  return sampleMean;
}
