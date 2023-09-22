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
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "entlib.h"
#include "globals.h"
#include "precision.h"
#include "health-tests.h"

// This test is useful for RO-based designs, in particular where each RO is of the same design
// (i.e., has the same number of delay elements).
// In such cases, the nominal frequencies may be close, so there is possibly an
// entrainment problem, both at the output and per-delay element levels.
//
// This Crosswise Repetition Count Test (CRCT) takes each 8-bit symbol, and treats each of the bits as parallel outputs of a noise source copy.
// There is an RCT on the literal outputs of each noise source copy, and also on the
// "crosswise" XOR of them, e.g., RO 3 with RO 7.
//
// The cross RCTs detect prolonged instances where a noise source copy pair is either persistently
// equal, or persistently complements of each other.
// In most systems, such crosswise runs are expected (simply due to the underlying deterministic
// portion of the RO behavior), but they should not persist in the long term (as the relative
// phases of the ROs are expected to independently drift).
//
// There are exactly 28 "cross" patterns that we need, but they are somewhat distributed.
// A more careful accounting would allow for running 36 total tests (28 "cross" RCTs + 8 literal RCTs)
// but this would complicate the per-data element processing.
// We can get all possible crosses by using the literal raw data (8 bits) + 4 of the "cross" shifted values.
//
// Note, using small values for CROSS_RCT_SHIFT_BYTES results in not testing all the possible crosswise patterns,
// and using more bytes just unnecessarily repeats tests.
//
// Even this results in 40 total tests, so there are still 4 repeated tests.
// We'll only report on the distinct ones.

//  Set the initial cross RCT state
void initCrossRCT(size_t baseRCTcutoff, size_t crossRCTcutoff, struct crossRCTstate *state) {
  state->RCT_Input = 0;

  for (unsigned int j = 0; j < CROSS_RCT_TESTS; j++) {
    state->RCT_C[j] = (j < 8) ? baseRCTcutoff : crossRCTcutoff;
    state->RCT_Failures[j] = 0;
    state->runCounts[j] = NULL;
    state->runCountLengths[j] = 0;
  }
  state->maxLiteralFailures = 0;
  state->maxCrossFailures = 0;
}

// A 1-byte circular shift function.
// Note: -rotate&7U is the same as 8-rotate, except when rotate=0 (where -rotate&7U is 0).
static uint8_t rotl8(uint8_t in, unsigned int rotate) {
  assert(rotate < 8);
  return (uint8_t)((in << rotate) | (in >> (-rotate&7U)));
}

// A function that rounds up to the next power of 2.
// Used to establish the size of memory allocation.
static size_t pow2Round(size_t x) {
  assert(x>0);
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x |= x >> 32;
  x++;
  assert(x>0);
  return x;
}

// To understand the logic of the RCT, it is useful to refer to
// SP 800-90B, Section 4.4.1. The variable names A, B, C all are consistent with that document.
// The "X" of SP 800-90B is one of the bits of the variable "base", which is either one of the bits of "in",
// or one of the XOR values from one of the in ^ rotl8(in, j) values.
bool crossRCT(uint8_t in, struct crossRCTstate *state) {
  bool RCT_Pass = true;
  uint64_t base;
  uint64_t diff;
  uint32_t literalFailures = 0;
  uint32_t crossFailures = 0;

  //Step 1 of the CRCT health test
  //This is for loop the "crossExpand" function in the specification
  // The format of the data byte is the literal raw data (`in`) as the bits [7:0]
  // in ^ rotl8(in, 1) is in the bits [15:8],
  // in ^ rotl8(in, 2) is in the bits [23:16]
  //  ...
  // in ^ rotl8(in, j) is in the bits [8*(j+1)-1:8*j]
  // Produce the cross patterns
  base = (uint64_t)in;
  for (unsigned int j = 1; j < CROSS_RCT_SHIFT_BYTES; j++) {
    base = base | (((uint64_t)(rotl8(in, j) ^ in)) << j * 8);
  }

  if (state->RCT_Input == 0) {
    // Step 2 of the CRCT health test
    for (unsigned int j = 0; j < CROSS_RCT_TESTS; j++) {
      state->RCT_B[j] = 1;
      if (state->runCounts[j]) state->runCounts[j][1] = 1;
    }
  } else {
    //Prep for step 4 of the CRCT health test.
    // Calculate the difference between the prior state and the current state.
    // On a per-bit basis, this yields either a 0 if the bits match,
    // or a 1 if they are distinct.
    diff = base ^ state->RCT_A;

    for (unsigned int j = 0; j < CROSS_RCT_TESTS; j++) { // loop in step 4 of the CRCT health test.
      if (((diff >> j) & 1U) == 1) { //step 4a
        //Step 4b(i) of the CRCT health test
        // Reset the run count for this test.
        state->RCT_B[j] = 1;
      } else {
        // The two bits are the same, so we extend the current run.
        //Step 4a(i) in the CRCT health test
        state->RCT_B[j]++;

        // If this would be flagged as an error, update the error counter.
        // This is step 4a(ii); in this context, we are only counting the total number of failures.
        if (state->RCT_B[j] >= state->RCT_C[j]) {
          state->RCT_Failures[j]++;
          RCT_Pass = false;
          if(j<8) literalFailures++;
          else crossFailures++;
        }
      }

      // This allows us to keep track of the per-symbol run length distribution, which is helpful for establishing the cutoff.
      // Record the run length (if we are doing that)
      if (state->runCounts[j]) {
        if(state->runCountLengths[j] <= state->RCT_B[j]) {
          size_t newLength = pow2Round(state->RCT_B[j]+1);
          size_t oldLength = state->runCountLengths[j];
          if(configVerbose > 2) fprintf(stderr, "Reallocate for bin %u: %zu -> %zu\n", j, oldLength, newLength);
          state->runCounts[j] = realloc(state->runCounts[j], sizeof(size_t)*newLength);
          assert(state->runCounts[j]!=NULL);

          //Clear out the new memory.
          for(size_t index=oldLength; index<newLength; index++) state->runCounts[j][index] = 0;

          //Note that we have more memory now...
          state->runCountLengths[j] = newLength;
        }
        state->runCounts[j][state->RCT_B[j]]++;
      }
    }
  }

  if(state->maxLiteralFailures < literalFailures) state->maxLiteralFailures = literalFailures;
  if(state->maxCrossFailures < crossFailures) state->maxCrossFailures = crossFailures;
  
  // Step 5 of the CRCT health test.
  // Update the reference data for all CROSS_RCT_TESTS tests.
  state->RCT_A = base;

  // Note that we processed an additional input.
  state->RCT_Input++;

  return RCT_Pass;
}


/* A generic RCT implementation. */

//  Set the initial RCT state
void initRCT(size_t RCTcutoff, struct RCTstate *state) {
  state->RCT_Input = 0;
  state->RCT_C = RCTcutoff;
  state->RCT_Failures = 0;
  state->runCounts = NULL;
  state->runCountsLength = 0;
}

// To understand the logic of the RCT, it is useful to refer to
// SP 800-90B, Section 4.4.1. The variable names A, B, C, and X all are consistent with that document.
bool RCT(uint64_t X, struct RCTstate *state) {
  bool RCT_Pass = true;

  if (state->RCT_Input == 0) {
    // Step 2 of the CRCT health test
    state->RCT_B = 1;
  } else {
    if( X != state->RCT_A ) {
      //Step 4b(i) of the RCT health test
      // Reset the run count for this test.
      state->RCT_B = 1;
    } else {
      // The two symbols are the same, so we extend the current run.
      //Step 4a(i) in the RCT health test
      state->RCT_B++;

      // If this would be flagged as an error, update the error counter.
      // This is step 4a(ii); in this context, we are only counting the total number of failures.
      if (state->RCT_B >= state->RCT_C) {
        state->RCT_Failures++;

        RCT_Pass = false;
      }
    }
  }

  // Some common processing
  // This allows us to keep track of the (per symbol) run distribution, which is helpful for establishing the cutoff.
  // Record the run length (if we are doing that)
  if (state->runCounts) {
    if(state->runCountsLength <= state->RCT_B) {
      size_t newLength = pow2Round(state->RCT_B+1);
      size_t oldLength = state->runCountsLength;
      if(configVerbose > 2) fprintf(stderr, "Reallocate run counts array %zu -> %zu\n", oldLength, newLength);
      state->runCounts = realloc(state->runCounts, sizeof(size_t)*newLength);
      assert(state->runCounts!=NULL);

      //Clear out the new memory.
      for(size_t index=oldLength; index<newLength; index++) state->runCounts[index] = 0;

      //Note that we have more memory now...
      state->runCountsLength = newLength;
    }

    //We now have space to record the run length, so do so.
    state->runCounts[state->RCT_B]++;
  }


  // Step 5 of the CRCT health test.
  // Update the reference data for all CROSS_RCT_TESTS tests.
  state->RCT_A = X;

  // Note that we processed an additional input.
  state->RCT_Input++;

  return RCT_Pass;
}

/* A generic APT implementation. */
//Start the whole thing.
void initAPT(size_t APT_cutoff, size_t APT_window, struct APTstate *state) {
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
	state->APT_i = APT_window; //Force a reset when we see the next input
	state->APTcounts = NULL;
}

bool APT(uint64_t in, struct APTstate *state) {
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
			if(state->APTcounts != NULL) state->APTcounts[state->APT_B]++;
			//Should we record a failure?
			if(!APT_Pass) state->APT_Failures ++;
		}
	}

	state->APT_Input++;

	return(APT_Pass);
}
