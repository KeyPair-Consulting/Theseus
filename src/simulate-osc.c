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
#include <getopt.h>  // for optarg, getopt, optind
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "fancymath.h"
#include "globals-inst.h"
#include "randlib.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "simulate-osc [-v] [-n <samples>] [-p <phase>] [-f] [-u] <ringOscFreq> <ringOscJitterSD> <sampleFreq>\n");
  fprintf(stderr, "<sampleFreq> Is either the sampling frequency or \"*\" followed by the number of nominal ring oscillations per sample cycle.\n");
  fprintf(stderr, "-v\tEnable verbose mode.\n");
  fprintf(stderr, "-s\tSend verbose mode output to stdout.\n");
  fprintf(stderr, "-n <samples>\tNumber of samples to generate (default: 1000000)\n");
  fprintf(stderr, "-p <phase>\tSpecify the initial phase of the ring oscillator from the interval [0,1) (relative to the sample clock). Default: generate randomly.\n");
  fprintf(stderr, "-d \t Make any RNG input deterministic.\n");
  fprintf(stderr, "-o \t Output deviation from deterministic output in addition to the actual output.\n");
  fprintf(stderr, "-f\tFudge the ringOscFreq using a normal distribution.\n");
  fprintf(stderr, "-u\tFudge the average intra-sample phase change. (Fix the number of cycles per sample, and select the ISP fractional part uniformly from [0, 0.25]).\n");
  fprintf(stderr, "Frequency values are in Hz.\n");
  fprintf(stderr, "Jitter Standard Deviation is in seconds (e.g. 1E-15) or a percentage of the ring oscillator period (ending with %%, e.g., 0.001%%).\n");
  fprintf(stderr, "Output is " STATDATA_STRING " integers; lsb is oscillator output. next bit (if enabled) tracks deviations from the deterministic value.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  struct randstate rstate;

  statData_t detOut, nonDetOut;
  statData_t *data;
  double oscPhase;
  double samplePhase;
  double oscFreq;
  double oscJitter;
  double sampleFreq;
  size_t dataSample, j;
  int opt;
  long long unsigned inint;
  double indouble;
  char *nextarg;
  bool configFudge;
  bool configISP;
  bool configOutputDeviations;
  double out1;
  double out2;

  dataSample = 1000000;
  configVerbose = 0;

  oscPhase = 2.0;
  samplePhase = 0.0;
  configFudge = false;
  configISP = false;
  configOutputDeviations = false;

  initGenerator(&rstate);

  while ((opt = getopt(argc, argv, "uodsvn:p:f")) != -1) {
    switch (opt) {
      case 's':
        if (dup2(fileno(stdout), fileno(stderr)) != fileno(stderr)) {
          perror("Can't dup stdout to stderr");
          exit(EX_OSERR);
        }
        break;
      case 'v':
        configVerbose++;
        break;
      case 'u':
        configISP = true;
        break;
      case 'f':
        configFudge = true;
        break;
      case 'o':
        configOutputDeviations = false;
        break;
      case 'p':
        indouble = strtod(optarg, &nextarg);
        if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (indouble >= 1.0)) {
          useageExit();
        }
        oscPhase = indouble;
        break;
      case 'n':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          useageExit();
        }
        dataSample = inint;
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

  if (argc != 3) {
    useageExit();
  }

  indouble = strtod(argv[0], &nextarg);
  if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
    useageExit();
  }
  oscFreq = indouble;

  indouble = strtod(argv[1], &nextarg);
  if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
    useageExit();
  }
  if ((nextarg != NULL) && (*nextarg == '%')) {
    if ((indouble < 0) || (indouble > 100)) {
      useageExit();
    }
    oscJitter = (1.0 / oscFreq) * (indouble / 100.0);
  } else {
    oscJitter = indouble;
  }

  if (*argv[2] == '*') {
    argv[2]++;
    indouble = strtod(argv[2], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      useageExit();
    }
    sampleFreq = oscFreq / indouble;
  } else {
    indouble = strtod(argv[2], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      useageExit();
    }
    sampleFreq = indouble;
  }

  seedGenerator(&rstate);

  if ((oscPhase >= 1.0) || (oscPhase < 0.0)) {
    oscPhase = randomUnit(&rstate);
  }

  if (configFudge) {
    // This perhaps makes more sense for actual use.
    out1 = -1.0;
    out2 = -1.0;
    while (out1 <= 0.0) {
      normalVariate(1 / oscFreq, 0.001 / (2.5758293035489008 * oscFreq), &out1, &out2, &rstate);
      if (out1 <= 0.0) {
        out1 = out2;
      }
    }

    // This should be a valid value now.
    oscFreq = 1 / out1;
  }

  if (configISP) {
    // For modeling, we want the entire phase space [0,.25) explored.
    // Note that divide by 4 only changes the exponent!
    out1 = randomUnit(&rstate) / 4;
    sampleFreq = oscFreq / (floor(oscFreq / sampleFreq) + out1);
  }

  if (oscJitter > 1 / oscFreq) {
    fprintf(stderr, "The simulation can't tolerate jitter that large. Did you forget a %%?\n");
    exit(EX_DATAERR);
  }

  if (configVerbose > 0) {
    double integerPart, fractionalPart;
    fprintf(stderr, "oscFreq: %.17g\n", oscFreq);
    fprintf(stderr, "oscJitter: %.17g\n", oscJitter);
    fprintf(stderr, "sampleFreq: %.17g\n", sampleFreq);
    fprintf(stderr, "Initial Phase: %.17g\n", oscPhase);
    fractionalPart = modf(oscFreq / sampleFreq, &integerPart);
    fprintf(stderr, "Complete RO cycles per sample: %.17g, Nu: %.17g\n", integerPart, fractionalPart);
    fprintf(stderr, "Normalized per-sample oscillator jitter: %.17g\n", oscJitter * oscFreq * sqrt(integerPart));
  }

  if ((data = malloc(sizeof(statData_t) * dataSample)) == NULL) {
    perror("Can't allocate internal buffer");
    exit(EX_OSERR);
  }

  for (j = 0; j < dataSample; j++) {
    if (configVerbose > 1) {
      fprintf(stderr, "Sample Phase: %.17g osc Phase: %.17g\n", samplePhase, oscPhase);
    }

    if (configOutputDeviations) {
      detOut = ringOscillatorNextDeterministicSample(oscFreq, oscPhase, sampleFreq, samplePhase);
      nonDetOut = ringOscillatorNextNonDeterministicSample(oscFreq, oscJitter, &oscPhase, sampleFreq, &samplePhase, &rstate);
      data[j] = (statData_t)(nonDetOut | ((detOut ^ nonDetOut) << 1));
    } else {
      nonDetOut = ringOscillatorNextNonDeterministicSample(oscFreq, oscJitter, &oscPhase, sampleFreq, &samplePhase, &rstate);
      data[j] = nonDetOut;
    }
  }

  if (fwrite(data, sizeof(statData_t), dataSample, stdout) != dataSample) {
    perror("Can't write to output");
    exit(EX_OSERR);
  }

  free(data);

  return EX_OK;
}
