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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <stdnoreturn.h>

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u64-jent-to-delta\n");
  fprintf(stderr, "input comes from stdin are in uint64_t, in the default jent delta format.\n");
  fprintf(stderr, "output sent to stdout is the number of nanoseconds represented by the delta.\n");
  exit(EX_USAGE);
}

static uint64_t mullerDeltaToNanosecondDelta(uint64_t delta) {
  // The upper 32 bits of the delta contains the number of seconds since the epoch. We do not (in general) expect this to roll over.
  // Note: if the time is adjusted between samples, then it may go negative, but these should be discarded, not treated as integer rollovers.
  const uint64_t secondsPlace = (delta >> 32) * 1000000000UL;
  const uint64_t nanosecondsPlace = delta & 0xFFFFFFFFUL;

  // Process the lower 32bits (the nanosecond count).
  // Did the _seconds_ place get borrowed from?
  // The nanoseconds place should normally be in the range [0, 999999999],
  // but in the instance where the second count is borrowed from,  the calculation then 2^32 was added (by borrowing from seconds place).
  // As a consequence, bit 31 (the high order bit in the 32-bit word) signals if borrowing occurred.
  if ((nanosecondsPlace & ((uint64_t)0x80000000UL)) == 0) {
    // No borrowing
    // Convert to a reasonable counter by just shift/multiply and add
    return secondsPlace + nanosecondsPlace;
  } else {
    // The second count was borrowed from; this is correct, so the shift/multiply for the seconds place is the same.
    // The lower bits, however, need to have the (wrong) borrow reversed (the original borrow was 2^32),
    // and the correct one applied (10^9).
    const uint64_t correctedNanosecondsPlace = 1000000000UL + nanosecondsPlace - ((uint64_t)0x0100000000UL);
    return secondsPlace + correctedNanosecondsPlace;
  }
}

int main(void) {
  uint64_t *input = NULL;
  uint64_t *nativeData = NULL;
  uint64_t *swappedData = NULL;
  uint64_t *outData = NULL;
  size_t datalen;
  size_t nativeSmallerCount = 0;

  if((datalen = readuint64file(stdin, &input))<1) {
    useageExit();
  }

  nativeData = malloc(sizeof(uint64_t) * datalen);
  swappedData = malloc(sizeof(uint64_t) * datalen);

  if ((nativeData == NULL) || (swappedData == NULL)) {
    perror("Can't allocate memory.");
    exit(EX_OSERR);
  }

  for (size_t i = 0; i < datalen; i++) {
    nativeData[i] = mullerDeltaToNanosecondDelta(input[i]);
    swappedData[i] = mullerDeltaToNanosecondDelta(reverse64(input[i]));
    if (nativeData[i] <= swappedData[i]) nativeSmallerCount++;
  }

  if (nativeSmallerCount >= (datalen - nativeSmallerCount)) {
    fprintf(stderr, "Native byte order seems better (%g)\n", (double)nativeSmallerCount / (double)datalen);
    outData = nativeData;
  } else {
    fprintf(stderr, "Swapped byte order seems better (%g)\n", (double)(datalen - nativeSmallerCount) / (double)datalen);
    outData = swappedData;
  }

  if (fwrite(outData, sizeof(uint64_t), datalen, stdout) != datalen) {
    perror("Can't write out data");
  }

  free(nativeData);
  free(swappedData);
  free(input);
}
