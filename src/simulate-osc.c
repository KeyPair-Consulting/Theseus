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
#include <string.h>

#include "fancymath.h"
#include "globals-inst.h"
#include "randlib.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "simulate-osc [-v] [-n <samples>] [-p <phase>] [-f] [-u] [-e <oscA>,<oscB>] <sampleFreq> <ringOscFreq_0> <ringOscJitterSD_0> ... [<ringOscFreq_{n-1}> <ringOscJitterSD_{n-1}>]\n");
  fprintf(stderr, "<sampleFreq> Is either the sampling frequency or \"*\" followed by the number of nominal ring oscillations per sample cycle of rinOsc_0.\n");
  fprintf(stderr, "<ringOscFreq_i> Is in Hz. If <ringOscFreq_i> is '0', then the oscillator is fixed..\n");
  fprintf(stderr, "-v\tEnable verbose mode.\n");
  fprintf(stderr, "-s\tSend verbose mode output to stdout.\n");
  fprintf(stderr, "-n <samples>\tNumber of samples to generate (default: 1000000)\n");
  fprintf(stderr, "-p <phase_n>,...<phase_1>\tSpecify the initial phase of the ring oscillator from the interval [0,1) (relative to the sample clock). Default: generate randomly.\n");
  fprintf(stderr, "-d \t Make any RNG input deterministic.\n");
  fprintf(stderr, "-f\tFudge the ringOscFreqs using a normal distribution.\n");
  fprintf(stderr, "-u\tWhen simulating a single oscillator, fudge the average intra-sample phase changes. (Fix the number of cycles per sample, and select the ISP fractional part uniformly from [0, 0.25]).\n");
  fprintf(stderr, "-e <oscA>,<oscB>\tEntrain <oscA> to <oscB> (that is, oscB now establishes the output of oscA).\n");
  fprintf(stderr, "\t\t\tNo tree structures allowed, so <oscA> cannot be entrained to another ring oscillator, and nothing can entrain <oscB>.\n");
  fprintf(stderr, "\t\t\tThe source oscillator can be inverted using a '~', so \"-e 1,~0\" denotes that the output of osc1 is the bitwise complement of osc0.\n");
  fprintf(stderr, "Frequency values are in Hz.\n");
  fprintf(stderr, "Jitter Standard Deviations are in seconds (e.g. 1E-15) or a percentage of the ring oscillator period (ending with %%, e.g., 0.001%%).\n");
  fprintf(stderr, "Output is " STATDATA_STRING " integers if the ROs fit, expanding to uint32_t if needed; lsb is the output of ringOsc_0, to msb ringOsc_{n-1}.\n");
  exit(EX_USAGE);
}

struct oscillatorState {
  double oscPhase;
  double samplePhase;
  double oscFreq;
  double oscJitter;
};

#define MAXOSC 32

int main(int argc, char *argv[]) {
  struct randstate rstate;

  statData_t nonDetOut;
  uint32_t *data;
  struct oscillatorState *roStates;
  size_t oscCount=0;
  double initOscPhase[MAXOSC];
  ssize_t oscEntrained[MAXOSC];
  double sampleFreq;
  size_t dataSample, j;
  int opt;
  long long unsigned inint;
  double indouble;
  char *nextarg;
  bool configFudge;
  bool configISP;
  ssize_t toOsc, fromOsc;
  bool invertOsc;
  uint8_t outputVector[MAXOSC]; 

  dataSample = 1000000;
  configVerbose = 0;

  for(size_t i=0; i<MAXOSC; i++) initOscPhase[i]=2.0;
  for(ssize_t i=0; i<MAXOSC; i++) oscEntrained[i]=i+1;
  configFudge = false;
  configISP = false;

  initGenerator(&rstate);

  while ((opt = getopt(argc, argv, "uwdsvn:p:fe:")) != -1) {
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
      case 'p':
        for(size_t i=0;i<MAXOSC;i++) {
          indouble = strtod(optarg, &nextarg);
          if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0) || (indouble >= 1.0)) {
            fprintf(stderr, "Phase out of range.\n");
            useageExit();
          }

          initOscPhase[i] = indouble;
          if(*nextarg == ',') {
            optarg = nextarg+1;
          } else {
            oscCount = i+1;
            break;
          }
        }
        break;
      case 'n':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL)) {
          fprintf(stderr, "Data count out of range.\n");
          useageExit();
        }
        dataSample = inint;
        break;
      case 'e':
        inint = strtoull(optarg, &nextarg, 10);
        if ((inint >= MAXOSC) || (errno == EINVAL) || (*nextarg != ',')) {
          fprintf(stderr, "Invalid oscillator identifier.\n");
          useageExit();
        }
        nextarg++;

        toOsc = (ssize_t)inint;

        if(*nextarg == '~') {
          invertOsc = true;
          nextarg++;
        } else {
          invertOsc = false;
        }

        inint = strtoull(nextarg, &nextarg, 10);
        if ((inint >= MAXOSC) || (errno == EINVAL)) {
          fprintf(stderr, "Invalid oscillator identifier.\n");
          useageExit();
        }
        fromOsc = (ssize_t)inint;

        if((oscEntrained[fromOsc] != (fromOsc+1)) || (oscEntrained[toOsc] != (toOsc+1))) {
          fprintf(stderr, "From osc must not itself be entrained and to osc must not already be entrained.\n");
          useageExit();
        }

        if(invertOsc) oscEntrained[toOsc] = -(fromOsc+1);
        else oscEntrained[toOsc] = fromOsc+1;
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

  if ((argc < 3) || (argc % 2 != 1)) {
    useageExit();
  }

  assert((oscCount == 0) || (((size_t)argc)>>1 == oscCount));
  oscCount = ((size_t)argc)>>1;
  assert(oscCount <= MAXOSC);

  for(size_t i=0; i<oscCount; i++) {
    if((oscEntrained[i] > (ssize_t)oscCount) || (oscEntrained[i] < -((ssize_t)oscCount))) {
      fprintf(stderr, "Oscillator index out of range.\n");
      useageExit();
    }
  }

  if ((roStates = malloc(oscCount*sizeof(struct oscillatorState))) == NULL) {
    perror("Can't allocate ro state");
    exit(EX_OSERR);
  }

  seedGenerator(&rstate);

  for(size_t i=0; i<oscCount; i++) {
    indouble = strtod(argv[1+2*i], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      fprintf(stderr, "Osc frequency out of range.\n");
      useageExit();
    }
    roStates[i].oscFreq = indouble;

    indouble = strtod(argv[2+2*i], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      fprintf(stderr, "Osc jitter out of range.\n");
      useageExit();
    }
    if ((nextarg != NULL) && (*nextarg == '%')) {
      if ((indouble < 0) || (indouble > 100)) {
      fprintf(stderr, "Osc jitter percent out of range.\n");
        useageExit();
      }
      roStates[i].oscJitter = (1.0 / roStates[i].oscFreq) * (indouble / 100.0);
    } else {
      roStates[i].oscJitter = indouble;
    }

    if ((initOscPhase[i] >= 1.0) || (initOscPhase[i] < 0.0)) {
      roStates[i].oscPhase = randomUnit(&rstate);
    } else {
      roStates[i].oscPhase = initOscPhase[i];
    }

    if (configFudge && (roStates[i].oscFreq > 0.0)) {
      // This perhaps makes more sense for actual use.
      double out1 = -1.0;
      double out2 = -1.0;
      while (out1 <= 0.0) {
        normalVariate(1.0 / roStates[i].oscFreq, 0.001 / (2.5758293035489008 * roStates[i].oscFreq), &out1, &out2, &rstate);
        if (out1 <= 0.0) {
          out1 = out2;
        }
      }

      // This should be a valid value now.
      roStates[i].oscFreq = 1.0 / out1;
    }

    if (roStates[i].oscJitter > (1.0 / roStates[i].oscFreq)) {
      fprintf(stderr, "The simulation can't tolerate jitter that large. Did you forget a %%?\n");
      exit(EX_DATAERR);
    }

     roStates[i].samplePhase = 0.0;
  }

  //Setup the sampling frequency
  if (*argv[0] == '*') {
    argv[0]++;
    indouble = strtod(argv[0], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      fprintf(stderr, "Sample frequency multiple out of range.\n");
      useageExit();
    }
    sampleFreq = roStates[0].oscFreq / indouble;
  } else {
    indouble = strtod(argv[0], &nextarg);
    if ((nextarg == optarg) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE) || (indouble < 0.0)) {
      fprintf(stderr, "Sample frequency out of range.\n");
      useageExit();
    }
    sampleFreq = indouble;
  }

  if (configISP) {
    double out1;
    //This only makes sense when there is only a single oscillator being modeled.
    if(oscCount != 1) {
      fprintf(stderr, "This mode only works with a single oscillator.\n");
     useageExit(); 
    }
    // For modeling, we want the entire phase space [0,.25) explored.
    // Note that divide by 4 only changes the exponent!
    out1 = randomUnit(&rstate) / 4.0;
    sampleFreq = roStates[0].oscFreq / (floor(roStates[0].oscFreq / sampleFreq) + out1);
  }

  if (configVerbose > 0) {
    double integerPart, fractionalPart;
    fprintf(stderr, "sampleFreq: %.17g\n", sampleFreq);
    for(size_t i=0; i<oscCount; i++) {
      if(roStates[i].oscFreq > 0.0) {
        if(oscEntrained[i] == (ssize_t)i+1) {
          fprintf(stderr, "osc[%zu] Freq: %.17g\n", i, roStates[i].oscFreq);
          fprintf(stderr, "osc[%zu] Jitter: %.17g\n", i, roStates[i].oscJitter);
          fprintf(stderr, "osc[%zu] Initial Phase: %.17g\n", i, roStates[i].oscPhase);
          fractionalPart = modf(roStates[i].oscFreq / sampleFreq, &integerPart);
          fprintf(stderr, "osc[%zu] Complete RO cycles per sample: %.17g, Nu: %.17g\n", i, integerPart, fractionalPart);
          fprintf(stderr, "osc[%zu] Normalized per-sample oscillator jitter: %.17g\n", i, roStates[i].oscJitter * roStates[i].oscFreq * sqrt(integerPart));
        } else {
          if(oscEntrained[i] > 0)
            fprintf(stderr, "osc[%zu] Output of osc %zu\n", i, (size_t)oscEntrained[i]-1);
          else
            fprintf(stderr, "osc[%zu] Output bitwise complement of osc %zu\n", i, (size_t)(-oscEntrained[i])-1);
        }
      } else {
        fprintf(stderr, "osc[%zu] does not oscillate.\n", i);
      }
    }
  }

  if ((data = malloc(sizeof(uint32_t) * dataSample)) == NULL) {
    perror("Can't allocate internal buffer");
    exit(EX_OSERR);
  }

  memset(data, 0, dataSample*sizeof(uint32_t));

  // Do the simulations.
  for (j = 0; j < dataSample; j++) {
    for(size_t i=0; i<oscCount; i++) {
      if (configVerbose > 1) {
        fprintf(stderr, "Sample Phase: %.17g osc[%zu] Phase: %.17g\n", roStates[i].samplePhase, oscCount-i, roStates[i].oscPhase);
      }
      outputVector[i] = ringOscillatorNextNonDeterministicSample(roStates[i].oscFreq, roStates[i].oscJitter, &(roStates[i].oscPhase), sampleFreq, &(roStates[i].samplePhase), &rstate);
    }

    //We have a complete vector, now apply the entrainment (if any)
    for(ssize_t i=(ssize_t)oscCount-1; i>=0; i--) {
      if(oscEntrained[i] == i+1) {
        nonDetOut = outputVector[i] & 0x01;
      } else {
        if(oscEntrained[i] < 0) {
          nonDetOut = (~(outputVector[(-oscEntrained[i])-1])) & 0x01;
        } else {
          nonDetOut = outputVector[oscEntrained[i]-1] & 0x01;
        }
      }
      data[j] = (data[j] << 1) | nonDetOut;
    }
  }

  if(oscCount > STATDATA_BITS) {
    fprintf(stderr, "Outputting uint32_t data...\n");
    if (fwrite(data, sizeof(uint32_t), dataSample, stdout) != dataSample) {
      perror("Can't write to output");
      exit(EX_OSERR);
    }
  } else {
    statData_t *sdData;
    fprintf(stderr, "Outputting statData_t data...\n");
    if ((sdData=malloc(dataSample*sizeof(statData_t))) == NULL) {
      perror("Can't allocate statData_t buffer");
      exit(EX_OSERR);
    }
    for(size_t i=0; i<dataSample; i++) sdData[i] = (statData_t)data[i];
    if (fwrite(sdData, sizeof(statData_t), dataSample, stdout) != dataSample) {
      perror("Can't write to output");
      exit(EX_OSERR);
    }
    free(sdData);
  }

  free(roStates);
  free(data);

  return EX_OK;
}
