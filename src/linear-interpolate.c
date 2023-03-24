/* This file is part of the Theseus distribution.
 * Copyright 2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "bootstrap.h"
#include "fancymath.h"
#include "globals-inst.h"
#include "precision.h"
#include "randlib.h"

/*Takes doubles from stdin and gives the mean*/
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "linear-interpolate [-v] [-i] [-x] <value>\n");
  fprintf(stderr, "Takes a set of points (x_1, y_1), ... (x_n, y_n), one point per line, from stdin and treats the points as a relation.\n");
  fprintf(stderr, "The relation is then forced to be functional by discarding point; by default, the point with the lowest value (y-value) is retained for each distinct argument (x-value), unless otherwise directed.\n");
  fprintf(stderr, "The points in the resulting function are then used to define and extension function f: [x_1, x_n] -> R, where the values are established through linear interpolation.\n");
  fprintf(stderr, "The reference data may include the arguments (x-values) INFINITY (or INF) and -INFINITY (or -INF) as end arguments (x_1 or x_n). These are a flag that the function should be extended as a constant function.\n");
  fprintf(stderr, "The value of such points must be equal to the nearest argument (x-value).\n");
  fprintf(stderr, "-v\tVerbose mode (can be used up to 3 times for increased verbosity).\n");
  fprintf(stderr, "-i\tAfter the described relation is turned into a function, the relation's coordinates are exchanged, and points are discarded from the resulting relation until it is again a function (this is in some sense an inverse function).\n");
  fprintf(stderr, "-x\tIf we encounter a relation with multiple equal arguments, keep the one with the largest value (y-value).\n");
  fprintf(stderr, "-c\tAssume that the function (or its inverse) is constant when out of the domain (or range, respectively).\n");
  exit(EX_USAGE);
}

// Check to see if x is in [z0, z1]
static inline bool inClosedInterval(double x, double z0, double z1) {
  return relEpsilonEqual(x, z0, ABSEPSILON, RELEPSILON, ULPEPSILON) || relEpsilonEqual(x, z1, ABSEPSILON, RELEPSILON, ULPEPSILON) || ((x > fmin(z0, z1)) && (x < fmax(z0, z1)));
}
// Check to see if x is in (z0, z1)
/*
static inline bool inOpenInterval(double x, double z0, double z1) {
  return (!relEpsilonEqual(x, z0, ABSEPSILON, RELEPSILON, ULPEPSILON) && !relEpsilonEqual(x, z1, ABSEPSILON, RELEPSILON, ULPEPSILON)) && ((x > fmin(z0, z1)) && (x < fmax(z0, z1)));
}
*/

/*Points are encoded as (x_j, y_j) are encoded as adjacent doubles in a single double array.*/
/*Sorting using this function yields a list of points, sorted by their argument (x-value), and secondarily sorted by their value (y-argument)*/
static int pointcompare(const void *in1, const void *in2) {
  const double *leftp, *rightp;
  bool lefteq = false, righteq = false;

  assert(in1 != NULL);
  assert(in2 != NULL);

  leftp = (const double *)in1;
  rightp = (const double *)in2;

  assert(!isnan(leftp[0]) && !isnan(leftp[1]) && !isnan(rightp[0]) && !isnan(rightp[1]));
  if (relEpsilonEqual(leftp[0], rightp[0], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    lefteq = true;
    if (relEpsilonEqual(leftp[1], rightp[1], ABSEPSILON, RELEPSILON, ULPEPSILON)) righteq = true;
  } else {
    lefteq = false;
  }

  if (lefteq) {
    // The first coordinates are equal, so this resolves to testing the second coordinates for equality
    if (righteq) {
      return 0;
    } else if (leftp[1] < rightp[1]) {
      return -1;
    } else {
      return 1;
    }
  } else {
    // The first coordinate are not equal.
    if (leftp[0] < rightp[0]) {
      return -1;
    } else {
      return 1;
    }
  }
}

// At this point, the points should be purely finite.
static void relationInverse(double *points, size_t n) {
  for (size_t j = 0; j < n; j++) {
    double temp = points[2 * j];
    points[2 * j] = points[2 * j + 1];
    points[2 * j + 1] = temp;
  }
}

// Presumed to be in sorted order
static size_t discardDuplicateArguments(double *points, size_t n, bool discardNonMinimum) {
  double *pnextout, *plast;
  size_t newCount = 0;

  pnextout = points;

  plast = points;
  if (discardNonMinimum) {
    // we need not copy in this case, as it is necessarily the first entry
    pnextout += 2;
    newCount++;
  }

  for (size_t j = 1; j < n; j++) {
    double *pcur = points + 2 * j;
    if (!relEpsilonEqual(plast[0], pcur[0], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      // These are not equal, so no matter what mode we are in, something needs to happen.
      if (discardNonMinimum) {
        // We are keeping only the smallest (thus first) value, which is the first after a transition.
        if (pcur != pnextout) memcpy(pnextout, pcur, sizeof(double) * 2);
      } else {
        // We are keeping only the largest (thus last) value, which is the first after a transition.
        if (plast != pnextout) memcpy(pnextout, plast, sizeof(double) * 2);
      }
      pnextout += 2;
      newCount++;
    } else {
      if (!relEpsilonEqual(plast[1], pcur[1], ABSEPSILON, RELEPSILON, ULPEPSILON)) fprintf(stderr, "Relation is not right-unique. Discarding ( %.17g, %.17g )\n", pcur[0], pcur[1]);
    }

    plast = pcur;
  }

  if (!discardNonMinimum) {
    if (plast != pnextout) memcpy(pnextout, plast, sizeof(double) * 2);
    newCount++;
    // we need not increment pnextout in this case, as we are done.
  }

  return newCount;
}

// Presumed to be in sorted order
static size_t compressConstantIntervals(double *points, size_t n) {
  double *pnextout, *plast;
  size_t newCount = 0;
  bool inConstantInterval = false;

  pnextout = points;

  plast = points;
  // we need not copy in this case, as it is necessarily the first entry
  pnextout += 2;
  newCount++;

  for (size_t j = 1; j < n; j++) {
    double *pcur = points + 2 * j;
    if (!relEpsilonEqual(plast[1], pcur[1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      if (inConstantInterval) {
        // We were in a constant interval, but the current value is different.
        // Write out the endpoint of the constant interval.
        if (plast != pnextout) memcpy(pnextout, plast, sizeof(double) * 2);
        pnextout += 2;
        newCount++;
        inConstantInterval = false;
      }
      // These are not equal so we copy them
      // We are keeping only the smallest and the largest argument for each constant value region
      if (pcur != pnextout) memcpy(pnextout, pcur, sizeof(double) * 2);
      pnextout += 2;
      newCount++;
    } else {
      inConstantInterval = true;
    }

    plast = pcur;
  }

  // We were in a constant interval, and the list just ended.
  // Write out the endpoint of the constant interval.
  if (inConstantInterval) {
    if (plast != pnextout) memcpy(pnextout, plast, sizeof(double) * 2);
    newCount++;
    // we need not increment pnextout in this case, as we are done.
  }

  return newCount;
}

// Assumes (and tests to verify) that the list is sorted.
static bool isStrictlyMonotonicUp(double *points, size_t n) {
  bool result = true;
  assert(n > 0);

  if (configVerbose > 1) fprintf(stderr, "Checking to see if the described function is monotonic up: ");

  for (size_t j = 0; j < n - 1; j++) {
    assert(!relEpsilonEqual(points[2 * j], points[2 * j + 2], ABSEPSILON, RELEPSILON, ULPEPSILON) && (points[2 * j] < points[2 * j + 2]));
    if ((points[2 * j + 1] > points[2 * j + 3]) || relEpsilonEqual(points[2 * j + 1], points[2 * j + 3], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      if (configVerbose > 2) fprintf(stderr, "(%.17g, %.17g) -> (%.17g, %.17g)\n", points[2 * j], points[2 * j + 1], points[2 * j + 2], points[2 * j + 3]);
      result = false;
    }
  }

  if (result) {
    if (configVerbose > 1) fprintf(stderr, "Yes.\n");
  } else {
    if (configVerbose > 1) fprintf(stderr, "so, No.\n");
  }

  return result;
}

static bool isStrictlyMonotonicDown(double *points, size_t n) {
  assert(n > 0);

  if (configVerbose > 1) fprintf(stderr, "Checking to see if the described function is monotonic down: ");

  for (size_t j = 0; j < n - 1; j++) {
    assert(!relEpsilonEqual(points[2 * j], points[2 * j + 2], ABSEPSILON, RELEPSILON, ULPEPSILON) && (points[2 * j] < points[2 * j + 2]));
    if ((points[2 * j + 1] < points[2 * j + 3]) || relEpsilonEqual(points[2 * j + 1], points[2 * j + 3], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      if (configVerbose > 2) fprintf(stderr, "(%.17g, %.17g) -> (%.17g, %.17g), so ", points[2 * j], points[2 * j + 1], points[2 * j + 2], points[2 * j + 3]);
      if (configVerbose > 1) fprintf(stderr, "No.\n");
      return false;
    }
  }

  if (configVerbose > 1) fprintf(stderr, "Yes.\n");

  return true;
}

static bool isStrictlyMonotonic(double *points, size_t n) {
  if (!relEpsilonEqual(points[1], points[2 * (n - 1) + 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    if (points[1] < points[2 * (n - 1) + 1]) {
      return isStrictlyMonotonicUp(points, n);
    } else {
      return isStrictlyMonotonicDown(points, n);
    }
  } else {
    return false;
  }
}

/*This works well for functions, but it doesn't deal well with some of the behaviors that you get with
 *function inverses.*/
static double linearInterpolate(double x, double *points, size_t n, bool constantExtend) {
  double y;
  assert(n > 0);

  // Is the input argument effectively equal to the argument of the first point?
  if (relEpsilonEqual(x, points[0], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    return points[1];
  }

  // Is the input argument actually smaller than the first point's argument?
  if (x < points[0]) {
      fprintf(stderr, "Argument is lower than the domain interval. ");
    if(constantExtend) {
      fprintf(stderr, "Extending function as a constant.\n");
      return points[1];
    } else {
      fprintf(stderr, "Exiting.\n");
      exit(EX_DATAERR);
    }
  }

  // Is the input argument effectively equal to the argument of the last point?
  if (relEpsilonEqual(x, points[2 * (n - 1)], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    return points[2 * (n - 1) + 1];
  }

  // Is the input argument larger than the last point's argument?
  if (x > points[2 * (n - 1)]) {
      fprintf(stderr, "Argument is higher than the domain interval. ");
    if(constantExtend) {
      fprintf(stderr, "Extending function as a constant.\n");
      return points[2 * (n - 1) + 1];
    } else {
      fprintf(stderr, "Exiting.\n");
      exit(EX_DATAERR);
    }
  }

  // At this point, we know that there is some interval that applies. Find it.
  for (size_t j = 1; j < n; j++) {
    double x1 = points[2 * j];
    double y1 = points[2 * j + 1];
    // Is it equal to the upper end point?
    if (relEpsilonEqual(x, x1, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      return y1;
    }

    if (x < x1) {
      // The jth point's first argument is greater than the input argument, so we can interpolate between the (j-1)th point and the jth point.
      double x0 = points[2 * (j - 1)];
      double y0 = points[2 * (j - 1) + 1];

      // This deals with constant intervals (included infinite length ones)
      if (relEpsilonEqual(y0, y1, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        return y1;
      }

      assert(x > x0);
      assert(x1 > x0);
      if (configVerbose > 1) fprintf(stderr, "interpolate between (%.17g, %.17g) and (%.17g, %.17g).\n", x0, y0, x1, y1);
      // This is the linear interpolation.
      // https://en.wikipedia.org/wiki/Linear_interpolation
      y = y0 + (x - x0) * ((y1 - y0) / (x1 - x0));
      assert(inClosedInterval(y, y0, y1));
      return y;
    }
  }

  // We should never reach here.
  assert(false);
}

static size_t inverseIntervals(double **inverses, double inputArgument, double *points, size_t numOfPoints) {
  size_t outlen = 0;
  double maxValue = -DBL_INFINITY;
  double minValue = DBL_INFINITY;
  bool includedInInterval = false;

  assert(inverses != NULL);

  if ((*inverses = calloc(numOfPoints + 2, sizeof(double) * 2)) == NULL) {
    perror("Can't get memory");
    exit(EX_OSERR);
  }

  for (size_t j = 0; j < numOfPoints - 1; j++) {
    // Here, we either add the left point, or a constant interval (which includes the right point).
    // If the right point is included in a constant interval, then we flag that using includedInInterval.
    if (points[2 * j + 1] > maxValue) maxValue = points[2 * j + 1];
    if (points[2 * j + 1] < minValue) minValue = points[2 * j + 1];
    if (inClosedInterval(inputArgument, points[2 * j + 1], points[2 * j + 3])) {
      if (relEpsilonEqual(points[2 * j + 1], points[2 * j + 3], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        // This is within this constant interval
        (*inverses)[2 * outlen] = points[2 * j];
        (*inverses)[2 * outlen + 1] = points[2 * j + 2];
        outlen++;
        includedInInterval = true;
      } else if (relEpsilonEqual(inputArgument, points[2 * j + 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        if (includedInInterval) {
          includedInInterval = false;
        } else {
          // This is the left interval
          (*inverses)[2 * outlen] = points[2 * j];
          (*inverses)[2 * outlen + 1] = points[2 * j];
          outlen++;
        }
      } else if (!relEpsilonEqual(inputArgument, points[2 * j + 3], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        // The value occurs within the interval. Use linear interpolation for the inverse
        double x0, x1, y0, y1, x, y;
        // We are calculating the interpolated inverse function, so we swap the x and y places.
        x = inputArgument;
        x0 = points[2 * j + 1];
        x1 = points[2 * j + 3];
        y0 = points[2 * j];
        y1 = points[2 * j + 2];
        if (configVerbose > 1) fprintf(stderr, "interpolate between (%.17g, %.17g) and (%.17g, %.17g).\n", x0, y0, x1, y1);
        // This is the linear interpolation.
        // https://en.wikipedia.org/wiki/Linear_interpolation
        y = y0 + (x - x0) * ((y1 - y0) / (x1 - x0));
        assert(inClosedInterval(y, y0, y1));
        (*inverses)[2 * outlen] = y;
        (*inverses)[2 * outlen + 1] = y;
        outlen++;
      }
    }
  }

  if (points[2 * (numOfPoints - 1) + 1] > maxValue) maxValue = points[2 * (numOfPoints - 1) + 1];
  if (points[2 * (numOfPoints - 1) + 1] < minValue) minValue = points[2 * (numOfPoints - 1) + 1];
  if (relEpsilonEqual(inputArgument, points[2 * (numOfPoints - 1) + 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    if (!includedInInterval) {
      // This is the left interval
      (*inverses)[2 * outlen] = points[2 * (numOfPoints - 1)];
      (*inverses)[2 * outlen + 1] = points[2 * (numOfPoints - 1)];
      outlen++;
    }
  }

  if (inputArgument > maxValue) {
    fprintf(stderr, "The value provided is greater than any value in the provided map.\n");
  }
  if (inputArgument < minValue) {
    fprintf(stderr, "The value provided is less than any value in the provided map.\n");
  }

  return outlen;
}

static void printInverseIntervals(double *inverses, size_t inverseCount) {
  for (size_t j = 0; j < inverseCount; j++) {
    if (relEpsilonEqual(inverses[2 * j], inverses[2 * j + 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      printf("%.17g\n", inverses[2 * j]);
    } else {
      printf("[%.17g, %.17g]\n", inverses[2 * j], inverses[2 * j + 1]);
    }
  }
}

int main(int argc, char *argv[]) {
  size_t numOfPoints;
  double *points = NULL;
  double *pStart;
  int opt;
  bool configInverse = false;
  bool configMinValue = true;
  bool configConstantExtend = false;
  double inputArgument;
  char *nextArgument;

  configVerbose = 0;

  assert(PRECISION(UINT_MAX) >= 32);

  if (argc < 2) {
    useageExit();
  }

  inputArgument = strtod(argv[argc - 1], &nextArgument);
  if ((errno == ERANGE) || !isfinite(inputArgument) || (nextArgument == argv[0])) {
    fprintf(stderr, "Provided argument is invalid or out of range.\n");
    exit(EX_DATAERR);
  }
  argc--;

  while ((opt = getopt(argc, argv, "vixc")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'i':
        configInverse = true;
        break;
      case 'x':
        configMinValue = false;
        break;
      case 'c':
        configConstantExtend = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  if (argc != optind) {
    useageExit();
  }

  if (configVerbose > 0) {
    if (configInverse) {
      fprintf(stderr, "Looking for argument that yields a value of approximately %.17g\n", inputArgument);
    } else {
      fprintf(stderr, "Looking for an approximate value for the argument %.17g\n", inputArgument);
    }
  }

  numOfPoints = readasciidoublepoints(stdin, &points);
  if (configVerbose > 0) fprintf(stderr, "Number of input points: %zu\n", numOfPoints);

  if (numOfPoints == 0) {
    fprintf(stderr, "Some number of points are expected to be input via stdin.\n");
    exit(EX_DATAERR);
  }
  assert(points != NULL);

  qsort(points, numOfPoints, sizeof(double) * 2, pointcompare);

  pStart = points;

  numOfPoints = discardDuplicateArguments(pStart, numOfPoints, configMinValue);
  if (configVerbose > 0) fprintf(stderr, "Number of non-duplicated points: %zu\n", numOfPoints);

  numOfPoints = compressConstantIntervals(pStart, numOfPoints);
  if (configVerbose > 0) fprintf(stderr, "Number of points after constant interval merging: %zu\n", numOfPoints);

  if (numOfPoints < 2) {
    fprintf(stderr, "At least two points are needed for linear interpolation.\n");
    exit(EX_DATAERR);
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
  if ((pStart[0] == -DBL_INFINITY) && !relEpsilonEqual(pStart[1], pStart[3], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    fprintf(stderr, "The function must be constant on intervals of the form (-INFINITY, x_0].\n");
    exit(EX_DATAERR);
  }

  if ((pStart[2 * (numOfPoints - 1)] == DBL_INFINITY) && !relEpsilonEqual(pStart[2 * (numOfPoints - 1) + 1], pStart[2 * (numOfPoints - 2) + 1], ABSEPSILON, RELEPSILON, ULPEPSILON)) {
    fprintf(stderr, "The function must be constant on intervals of the form (x_{n-1}, INFINITY].\n");
    exit(EX_DATAERR);
  }
#pragma GCC diagnostic pop

  if (configInverse) {
    if (isStrictlyMonotonic(pStart, numOfPoints)) {
      // It is a monotonic function, so the inverse is guaranteed to be a single point.
      relationInverse(pStart, numOfPoints);
      qsort(pStart, numOfPoints, sizeof(double) * 2, pointcompare);
      numOfPoints = discardDuplicateArguments(pStart, numOfPoints, configMinValue);
      if (configVerbose > 0) fprintf(stderr, "Number of non-duplicated inverse points: %zu\n", numOfPoints);

      printf("%.17g\n", linearInterpolate(inputArgument, points, numOfPoints, configConstantExtend));
    } else {
      // This isn't a monotonic function, so we're going to have to be careful here.
      double *inverses;
      size_t inverseCount;
      // Format for inverseCount is entries of the form (a, b) denoting an inverse interval of [ a, b ]
      // Discrete points are indicated by returning the interval [ a, a ], which indicates the isolated point a is an inverse.
      inverseCount = inverseIntervals(&inverses, inputArgument, points, numOfPoints);
      assert((inverses != NULL) || (inverseCount == 0));
      if (inverses != NULL) {
        printInverseIntervals(inverses, inverseCount);
        free(inverses);
      }
    }
  } else {
    // This isn't the inverse, so we're all set.
    printf("%.17g\n", linearInterpolate(inputArgument, points, numOfPoints, configConstantExtend));
  }

  free(points);
  return (EX_OK);
}
