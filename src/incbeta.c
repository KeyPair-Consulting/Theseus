/*
 * zlib License
 *
 * Regularized Incomplete Beta Function
 *
 * Copyright (c) 2016, 2017 Lewis Van Winkle
 * http://CodePlea.com
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* This file is included as part of the Theseus distribution.
 * Some changes Copyright 2020 Joshua E. Hill <josh@keypair.us>
 *
 * Author(s)
 * Lewis Van Winkle, http://CodePlea.com
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */

#include "incbeta.h"
#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "globals.h"

#define STOP 1.0e-8
#define TINY 1.0e-30

double incbeta(double a, double b, double x) {
  double lbeta_ab;
  double front;
  double f = 1.0, c = 1.0, d = 0.0;
  int i, m;
  double numerator;
  double cd;
  double denom;

  assert(x >= 0);
  assert(x <= 1.0);
  assert(fabs(a) > 0);

  assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
  feclearexcept(FE_ALL_EXCEPT);

  /*The continued fraction converges nicely for x < (a+1)/(a+b+2)*/

  denom = (a + b + 2.0);
  assert(fabs(denom) > 0.0);
  if (x > (a + 1.0) / denom) {
    return (1.0 - incbeta(b, a, 1.0 - x)); /*Use the fact that beta is symmetrical.*/
  }

  /*Find the first part before the continued fraction.*/

  lbeta_ab = lgamma(a) + lgamma(b) - lgamma(a + b);
  front = exp(log(x) * a + log(1.0 - x) * b - lbeta_ab) / a;
  /*Use Lentz's algorithm to evaluate the continued fraction.*/

  for (i = 0; i <= 1073; ++i) {
    m = i / 2;

    if (i == 0) {
      numerator = 1.0; /*First numerator is 1.0.*/
    } else if (i % 2 == 0) {
      denom = (a + 2.0 * m - 1.0) * (a + 2.0 * m);
      assert(fabs(denom) > 0.0);
      numerator = (m * (b - m) * x) / denom; /*Even term.*/
    } else {
      denom = (a + 2.0 * m) * (a + 2.0 * m + 1);
      assert(fabs(denom) > 0.0);
      numerator = -((a + m) * (a + b + m) * x) / denom; /*Odd term.*/
    }

    /*Do an iteration of Lentz's algorithm.*/
    d = 1.0 + numerator * d;
    if (fabs(d) < TINY) d = TINY;
    d = 1.0 / d;

    assert(fabs(c) >= TINY);
    c = 1.0 + numerator / c;
    if (fabs(c) < TINY) c = TINY;

    cd = c * d;
    f *= cd;

    /*Check for stop.*/
    if (fabs(1.0 - cd) < STOP) {
      int exceptions;
      double result = front * (f - 1.0);

      exceptions = fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW);
      if (exceptions & FE_UNDERFLOW) {
        if (configVerbose > 1) fprintf(stderr, "Expected underflow in incbeta.");
        if (result < DBL_MIN) {
          result = 0.0;
          if (configVerbose > 1) fprintf(stderr, ".. Setting result to 0.");
        }
        if (configVerbose > 1) fprintf(stderr, "\n");

        feclearexcept(FE_UNDERFLOW);
        exceptions &= (~FE_UNDERFLOW);
      }

      if (exceptions != 0) {
        fprintf(stderr, "Math error in incbeta code: ");
        if (exceptions & FE_INVALID) {
          fprintf(stderr, "FE_INVALID ");
        }
        if (exceptions & FE_DIVBYZERO) {
          fprintf(stderr, "Divided by 0 ");
        }
        if (exceptions & FE_OVERFLOW) {
          fprintf(stderr, "Found an overflow ");
        }
        fprintf(stderr, "\n");
        exit(EX_DATAERR);
      }

      assert(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW) == 0);
      return result;
    }
  }

  fprintf(stderr, "incbeta result did not converge\n");
  exit(EX_DATAERR);
}
