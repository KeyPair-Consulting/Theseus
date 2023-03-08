/* This file is part of the Theseus distribution.
 * Copyright 2023 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <errno.h>
#include <getopt.h>  // for getopt, optind
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>  // for uint32_t
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-health-tests [-v] -R <value> -A <value> -W <value> <infile>\n");
  fprintf(stderr, "Runs the SP 800-90B APT and RCT health tests and reports the number of failures.\n");
  fprintf(stderr, "-v Increase verbosity. Can be used multiple times.\n");
  fprintf(stderr, "-R <value>\tThe RCT cutoff value is <value>.\n");
  fprintf(stderr, "-A <value>\tThe APT cutoff value is <value>.\n");
  fprintf(stderr, "-W <value>\tThe APT window value is <value>.\n");
  exit(EX_USAGE);
}

struct healthTestState {
	//RCT state
	size_t RCT_Input;
	size_t RCT_C;
	uint64_t RCT_A;
	size_t RCT_B;
	size_t RCT_Failures;

	//APT state
	size_t APT_Input;
	size_t APT_Window_Count;
	size_t APT_W;
	size_t APT_C;
	size_t APT_i;
	uint64_t APT_A;
	size_t APT_B;
	size_t APT_Failures;
};

//Start the whole thing.
static void initHealthTests(size_t RCT_cutoff, size_t APT_cutoff, size_t APT_window, struct healthTestState *state) {
	//Init RCT state
	state->RCT_C = RCT_cutoff;
	state->RCT_Input = 0;
	state->RCT_Failures=0;

	//Init APT state
	//Silence erroneous "possible use of uninitialized variable" nonsense.
	state->APT_A=0;
	state->APT_B=0;
	//Actually useful initialization
	state->APT_C = APT_cutoff;
	state->APT_W = APT_window;
	state->APT_Input = 0;
	state->APT_Failures = 0;
	state->APT_Window_Count = 0;
	state->APT_i = state->APT_W; //Force a reset the next input
}

static bool RCT_Test(uint64_t in, struct healthTestState *state) {
	bool RCT_Pass = true;

	if(state->RCT_Input == 0) {
		state->RCT_A = in;
		state->RCT_B = 1;
	} else {
		if(in == state->RCT_A) {
			state->RCT_B++;
			if(state->RCT_B >= state->RCT_C) RCT_Pass = false;
		} else {
			state->RCT_A = in;
			state->RCT_B = 1;
		}
	}
	if(!RCT_Pass) {
		state->RCT_Failures ++;
	}

	state->RCT_Input++;

	return RCT_Pass;
}

static bool APT_Test(uint64_t in, struct healthTestState *state) {
	bool APT_Pass = true;

	if(state->APT_i >= state->APT_W) {
		state->APT_A = in;
		state->APT_B = 1;
		state->APT_i = 1;
	} else {
		//This is the ith iteration of the loop
		if(state->APT_A == in) {
			state->APT_B++;
		}

		if(state->APT_B >= state->APT_C) {
			APT_Pass = false;
		}

		//Update i for the next round
		state->APT_i++;
		if(state->APT_i >= state->APT_W) {
			state->APT_Window_Count++;
			//Should we record a failure?
			if(!APT_Pass) state->APT_Failures ++;
		}
	}

	state->APT_Input++;

	return(APT_Pass);
}

//Returns an indication if the data should be credited with entropy
static bool performHealthTests(uint64_t in, struct healthTestState *state) {
	bool APT_Result, RCT_Result;

	RCT_Result = RCT_Test(in, state);
	APT_Result = APT_Test(in, state);
	
	return RCT_Result && APT_Result;
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  size_t configAPTC;
  size_t configAPTW;
  size_t configRCTC;
  int opt;
  unsigned long long inint;
  struct healthTestState healthTests;
  size_t RCT_Test_Count;

  configVerbose = 0;
  configAPTC = 0;
  configAPTW = 0;
  configRCTC = 0;

  while ((opt = getopt(argc, argv, "vR:A:W:")) != -1) {
    switch (opt) {
      case 'v':
        configVerbose++;
        break;
      case 'R':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configRCTC = (size_t)inint;
        break;
      case 'A':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAPTC = (size_t)inint;
        break;
      case 'W':
        inint = strtoull(optarg, NULL, 0);
        if ((inint == ULLONG_MAX) || (errno == EINVAL) || (inint > SIZE_MAX)) {
          useageExit();
        }
        configAPTW = (size_t)inint;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 1) {
    useageExit();
  }

  if ((infp = fopen(argv[0], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  datalen = readuint32file(infp, &data);
  assert(data != NULL);

  if (configVerbose > 0) {
    fprintf(stderr, "Read in %zu integers\n", datalen);
  }
  if (fclose(infp) != 0) {
    perror("Couldn't close input data file");
    exit(EX_OSERR);
  }

  assert(datalen > 0);

  initHealthTests(configRCTC, configAPTC, configAPTW, &healthTests);
  for(size_t i=0; i<datalen; i++) {
    performHealthTests((uint64_t)data[i], &healthTests);
  }

  printf("APT: Failure Rate: %zu / %zu = %g", healthTests.APT_Failures, healthTests.APT_Window_Count, ((double)(healthTests.APT_Failures))/((double)(healthTests.APT_Window_Count)));
  if(healthTests.APT_Failures > 0) {
    printf(" ≈ 2^%g\n", log2(((double)(healthTests.APT_Failures))/((double)(healthTests.APT_Window_Count))));
  } else {
    printf("\n");
  }

  assert(healthTests.RCT_Input >= healthTests.RCT_C);
  RCT_Test_Count = healthTests.RCT_Input-healthTests.RCT_C+1;
  printf("RCT: Failure Rate: %zu / %zu = %g", healthTests.RCT_Failures, RCT_Test_Count, ((double)(healthTests.RCT_Failures))/((double)RCT_Test_Count));
  if(healthTests.RCT_Failures > 0) {
    printf(" ≈ 2^%g\n", log2(((double)(healthTests.RCT_Failures))/((double)RCT_Test_Count)));
  } else {
    printf("\n");
  }

  free(data);
  return EX_OK;
}

