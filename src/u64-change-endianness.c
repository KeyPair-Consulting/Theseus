/* This file is part of the Theseus distribution.
 * Copyright 2021 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
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
  size_t datalen;

  if ((datalen = readuint64file(stdin, &input)) < 1) {
    perror("No data read.");
    exit(EX_DATAERR);
  }

  for (size_t i = 0; i < datalen; i++) {
    input[i] = reverse64(input[i]);
  }

  if (fwrite(input, sizeof(uint64_t), datalen, stdout) != datalen) {
    perror("Can't write out data");
  }

  free(input);
}
