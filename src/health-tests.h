#ifndef HEALTH_TESTS_H
#define HEALTH_TESTS_H

#define CROSS_RCT_SHIFT_BYTES 5U
#define CROSS_RCT_TESTS 36

// CrossRCT state.
// The meaning of A, B, and C are as in SP 800-90B, Section 4.4.1.
// A is the reference symbol being compared against (in this case, a vector of 40 single-bit symbols, comprised of 8 literal values and 32 crosswise values)
// B is the number of times that the current symbol has been encountered consecutively (i.e., we have encountered a length-B run of the relevant symbol)
// C is the cutoff (the test is judged a failure if a run of C or more symbols is found)
struct crossRCTstate {
  size_t RCT_Input;
  size_t RCT_C[CROSS_RCT_TESTS];
  uint64_t RCT_A;
  size_t RCT_B[CROSS_RCT_TESTS];
  size_t RCT_Failures[CROSS_RCT_TESTS];
  size_t *runCounts[CROSS_RCT_TESTS];
  size_t runCountLengths[CROSS_RCT_TESTS];
  uint32_t maxLiteralFailures;
  uint32_t maxCrossFailures;
};

//  Set the initial cross RCT state
void initCrossRCT(size_t baseRCTcutoff, size_t crossRCTcutoff, struct crossRCTstate *state);
bool crossRCT(uint8_t in, struct crossRCTstate *state);

// RCT state.
// The meaning of A, B, and C are as in SP 800-90B, Section 4.4.1.
// A is the reference symbol being compared against
// B is the number of times that the current symbol has been encountered consecutively (i.e., we have encountered a length-B run of the relevant symbol)
// C is the cutoff (the test is judged a failure if a run of C or more symbols is found)
struct RCTstate {
  size_t RCT_Input;
  size_t RCT_C;
  uint64_t RCT_A;
  size_t RCT_B;
  size_t RCT_Failures;
  size_t *runCounts;
  size_t runCountsLength;
};

void initRCT(size_t RCTcutoff, struct RCTstate *state);
//X is the symbol being input to the health test.
bool RCT(uint64_t X, struct RCTstate *state);

//APT state
struct APTstate {
	size_t APT_Input;
	size_t APT_Window_Count;
	size_t APT_W;
	size_t APT_C;
	size_t APT_i;
	uint64_t APT_A;
	size_t APT_B;
	size_t APT_Failures;
	size_t *APTcounts;
};

void initAPT(size_t APT_cutoff, size_t APT_window, struct APTstate *state);
bool APT(uint64_t in, struct APTstate *state);
#endif
