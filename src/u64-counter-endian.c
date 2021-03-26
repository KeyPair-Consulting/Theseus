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

#include "binio.h"
#include "binutil.h"
#include "globals-inst.h"

int main(void) {
  uint64_t *input = NULL;
  uint64_t *swappedData = NULL;
  uint64_t *outData = NULL;
  size_t datalen;
  size_t nativeSmallerCount = 0;

  if ((datalen = readuint64file(stdin, &input)) < 1) {
    perror("No data read.");
    exit(EX_DATAERR);
  }

  swappedData = malloc(sizeof(uint64_t) * datalen);

  if (swappedData == NULL) {
    perror("Can't allocate memory.");
    exit(EX_OSERR);
  }

  for (size_t i = 0; i < datalen; i++) {
    swappedData[i] = reverse64(input[i]);
    if (input[i] <= swappedData[i]) nativeSmallerCount++;
  }

  if (nativeSmallerCount >= (datalen - nativeSmallerCount)) {
    fprintf(stderr, "Native byte order seems better (%g)\n", (double)nativeSmallerCount / (double)datalen);
    outData = input;
  } else {
    fprintf(stderr, "Swapped byte order seems better (%g)\n", (double)(datalen - nativeSmallerCount) / (double)datalen);
    outData = swappedData;
  }

  if (fwrite(outData, sizeof(uint64_t), datalen, stdout) != datalen) {
    perror("Can't write out data");
  }

  free(swappedData);
  free(input);
}
