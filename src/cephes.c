/*
 * Cephes Math Library Release 2.8:  June, 2000
 * Copyright 1984, 1995, 2000 by Stephen L. Moshier
 *
 * This software is derived from the Cephes Math Library and is
 * incorporated herein by permission of the author.
 *
 * Copyright (c) 1984, 1987, 1989, 2000 by Stephen L. Moshier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the organization nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* The author allowed for its use under the 3-clause BSD license
 * https://raw.githubusercontent.com/deepmind/torch-cephes/master/LICENSE.txt
 * https://lists.debian.org/debian-legal/2004/12/msg00295.html
 */

/* This file is included as part of the Theseus distribution.
 * Author(s)
 * Stephen L. Moshier
 * Some small changes by:
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <float.h>
#include <math.h>
#include <stdio.h>

#include "cephes.h"
#include "fancymath.h"
#include "globals.h"

static double MACHEP = 1.11022302462515654042E-16;  // 2**-53
static double MAXLOG = 7.09782712893383996732224E2;  // log(MAXNUM)
static double MAXNUM = 1.7976931348623158E308;  // 2**1024*(1-MACHEP)
static double PI = 3.14159265358979323846;  // pi, duh!

static double big = 4.503599627370496e15;
static double biginv = 2.22044604925031308085e-16;

static int sgngam = 0;

/* A[]: Stirling's formula expansion of log gamma
 * B[], C[]: log gamma function between 2 and 3
 */

static double A[] = {8.11614167470508450300E-4, -5.95061904284301438324E-4, 7.93650340457716943945E-4, -2.77777777730099687205E-3, 8.33333333333331927722E-2};
static double B[] = {-1.37825152569120859100E3, -3.88016315134637840924E4, -3.31612992738871184744E5, -1.16237097492762307383E6, -1.72173700820839662146E6, -8.53555664245765465627E5};
static double C[] = {
    /* 1.00000000000000000000E0, */
    -3.51815701436523470549E2, -1.70642106651881159223E4, -2.20528590553854454839E5, -1.13933444367982507207E6, -2.53252307177582951285E6, -2.01889141433532773231E6};

#define MAXLGM 2.556348e305

static double cephes_polevl(double x, double *coef, int N) {
  double ans;
  int i;
  double *p;

  p = coef;
  ans = *p++;
  i = N;

  do {
    ans = ans * x + *p++;
  } while (--i);

  return ans;
}

static double cephes_p1evl(double x, double *coef, int N) {
  double ans;
  double *p;
  int i;

  p = coef;
  ans = x + *p++;
  i = N - 1;

  do {
    ans = ans * x + *p++;
  } while (--i);

  return ans;
}

/* Logarithm of gamma function */
static double cephes_lgam(double x) {
  double p, q, u, w, z;
  int i;

  sgngam = 1;

  if (x < -34.0) {
    q = -x;
    w = cephes_lgam(q); /* note this modifies sgngam! */
    p = floor(q);

    if (relEpsilonEqual(p, q, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      goto loverf;
    }

    i = (int)p;  // Note, p is the output of floor.

    if ((i & 1) == 0) {
      sgngam = -1;
    } else {
      sgngam = 1;
    }

    z = q - p;

    if (z > 0.5) {
      p += 1.0;
      z = p - q;
    }

    z = q * sin(PI * z);

    if (relEpsilonEqual(z, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      goto loverf;
    }

    /*      z = log(PI) - log( z ) - w;*/
    z = log(PI) - log(z) - w;
    return z;
  }

  if (x < 13.0) {
    z = 1.0;
    p = 0.0;
    u = x;

    while (u >= 3.0) {
      p -= 1.0;
      u = x + p;
      z *= u;
    }

    while (u < 2.0) {
      if (relEpsilonEqual(u, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
        goto loverf;
      }

      z /= u;
      p += 1.0;
      u = x + p;
    }

    if (z < 0.0) {
      sgngam = -1;
      z = -z;
    } else {
      sgngam = 1;
    }

    if (relEpsilonEqual(u, 2.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      return (log(z));
    }

    p -= 2.0;
    x = x + p;
    p = x * cephes_polevl(x, B, 5) / cephes_p1evl(x, (double *)C, 6);

    return log(z) + p;
  }

  if (x > MAXLGM) {
  loverf:
    fprintf(stderr, "lgam: OVERFLOW\n");

    return sgngam * MAXNUM;
  }

  q = (x - 0.5) * log(x) - x + log(sqrt(2 * PI));

  if (x > 1.0e8) {
    return q;
  }

  p = 1.0 / (x * x);

  if (x >= 1000.0)
    q += ((7.9365079365079365079365e-4 * p - 2.7777777777777777777778e-3) * p + 0.0833333333333333333333) / x;
  else {
    q += cephes_polevl(p, A, 4) / x;
  }

  return q;
}

static double cephes_igam(double a, double x) {
  double ans, ax, c, r;

  if ((x <= 0) || (a <= 0)) {
    return 0.0;
  }

  if ((x > 1.0) && (x > a)) {
    return 1.e0 - cephes_igamc(a, x);
  }

  /* Compute  x**a * exp(-x) / gamma(a)  */
  ax = a * log(x) - x - cephes_lgam(a);

  if (ax < -MAXLOG) {
    fprintf(stderr, "igam: UNDERFLOW\n");
    return 0.0;
  }

  ax = exp(ax);

  /* power series */
  r = a;
  c = 1.0;
  ans = 1.0;

  do {
    r += 1.0;
    c *= x / r;
    ans += c;
  } while (c / ans > MACHEP);

  return ans * ax / a;
}
static double s2pi = 2.50662827463100050242E0;

/* approximation for 0 <= |y - 0.5| <= 3/8 */
static double P0[5] = {
    -5.99633501014107895267E1, 9.80010754185999661536E1, -5.66762857469070293439E1, 1.39312609387279679503E1, -1.23916583867381258016E0,
};
static double Q0[8] = {
    /* 1.00000000000000000000E0,*/
    1.95448858338141759834E0, 4.67627912898881538453E0, 8.63602421390890590575E1, -2.25462687854119370527E2, 2.00260212380060660359E2, -8.20372256168333339912E1, 1.59056225126211695515E1, -1.18331621121330003142E0,
};

/* Approximation for interval z = sqrt(-2 log y ) between 2 and 8
 * i.e., y between exp(-2) = .135 and exp(-32) = 1.27e-14.
 */
static double P1[9] = {
    4.05544892305962419923E0, 3.15251094599893866154E1, 5.71628192246421288162E1, 4.40805073893200834700E1, 1.46849561928858024014E1, 2.18663306850790267539E0, -1.40256079171354495875E-1, -3.50424626827848203418E-2, -8.57456785154685413611E-4,
};
static double Q1[8] = {
    /*  1.00000000000000000000E0,*/
    1.57799883256466749731E1, 4.53907635128879210584E1, 4.13172038254672030440E1, 1.50425385692907503408E1, 2.50464946208309415979E0, -1.42182922854787788574E-1, -3.80806407691578277194E-2, -9.33259480895457427372E-4,
};

/* Approximation for interval z = sqrt(-2 log y ) between 8 and 64
 * i.e., y between exp(-32) = 1.27e-14 and exp(-2048) = 3.67e-890.
 */

static double P2[9] = {
    3.23774891776946035970E0, 6.91522889068984211695E0, 3.93881025292474443415E0, 1.33303460815807542389E0, 2.01485389549179081538E-1, 1.23716634817820021358E-2, 3.01581553508235416007E-4, 2.65806974686737550832E-6, 6.23974539184983293730E-9,
};
static double Q2[8] = {
    /*  1.00000000000000000000E0,*/
    6.02427039364742014255E0, 3.67983563856160859403E0, 1.37702099489081330271E0, 2.16236993594496635890E-1, 1.34204006088543189037E-2, 3.28014464682127739104E-4, 2.89247864745380683936E-6, 6.79019408009981274425E-9,
};

double cephes_ndtri(double y0) {
  double x, y, z, y2, x0, x1;
  int code;

  if (y0 <= 0.0) {
    fprintf(stderr, "ndtri: Domain error\n");
    return -MAXNUM;
  }

  if (y0 >= 1.0) {
    fprintf(stderr, "ndtri: Domain error\n");
    return MAXNUM;
  }

  code = 1;
  y = y0;
  if (y > (1.0 - 0.13533528323661269189)) { /* 0.135... = exp(-2) */
    y = 1.0 - y;
    code = 0;
  }

  if (y > 0.13533528323661269189) {
    y = y - 0.5;
    y2 = y * y;
    x = y + y * (y2 * cephes_polevl(y2, P0, 4) / cephes_p1evl(y2, Q0, 8));
    x = x * s2pi;
    return (x);
  }

  x = sqrt(-2.0 * log(y));
  x0 = x - log(x) / x;

  z = 1.0 / x;
  if (x < 8.0) { /* y > exp(-32) = 1.2664165549e-14 */
    x1 = z * cephes_polevl(z, P1, 8) / cephes_p1evl(z, Q1, 8);
  } else {
    x1 = z * cephes_polevl(z, P2, 8) / cephes_p1evl(z, Q2, 8);
  }
  x = x0 - x1;
  if (code != 0) {
    x = -x;
  }
  return (x);
}

double cephes_igamc(double a, double x) {
  double ans, ax, c, yc, r, t, y, z;
  double pk, pkm1, pkm2, qk, qkm1, qkm2;

  if ((x <= 0) || (a <= 0)) {
    return (1.0);
  }

  if ((x < 1.0) || (x < a)) {
    return (1.e0 - cephes_igam(a, x));
  }

  ax = a * log(x) - x - cephes_lgam(a);

  if (ax < -MAXLOG) {
    fprintf(stderr, "igamc: UNDERFLOW\n");
    return 0.0;
  }

  ax = exp(ax);

  /* continued fraction */
  y = 1.0 - a;
  z = x + y + 1.0;
  c = 0.0;
  pkm2 = 1.0;
  qkm2 = x;
  pkm1 = x + 1.0;
  qkm1 = z * x;
  ans = pkm1 / qkm1;

  do {
    c += 1.0;
    y += 1.0;
    z += 2.0;
    yc = y * c;
    pk = pkm1 * z - pkm2 * yc;
    qk = qkm1 * z - qkm2 * yc;

    if (!relEpsilonEqual(qk, 0.0, ABSEPSILON, RELEPSILON, ULPEPSILON)) {
      r = pk / qk;
      t = fabs((ans - r) / r);
      ans = r;
    } else {
      t = 1.0;
    }

    pkm2 = pkm1;
    pkm1 = pk;
    qkm2 = qkm1;
    qkm1 = qk;

    if (fabs(pk) > big) {
      pkm2 *= biginv;
      pkm1 *= biginv;
      qkm2 *= biginv;
      qkm1 *= biginv;
    }
  } while (t > MACHEP);

  return ans * ax;
}
