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
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-regress-to-mean <filename> <k>\n");
  fprintf(stderr, "Calculate the mean, and then force each k-block to have this mean, and then round the resulting values.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  long double globalMean = 0.0L;
  char *nextc;
  size_t k;
  size_t blockCount;

  if (argc != 3) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if((datalen = readuint32file(infp, &data))<1) {
    useageExit();
  }

  assert(data != NULL);
  k = strtoul(argv[2], &nextc, 0);
  if (*nextc != '\0') {
    useageExit();
  }

  fprintf(stderr, "Read in %" PRId64 " uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  // Make k divide datalen. (discard other data)
  blockCount = datalen / k;
  datalen -= (datalen % k);

  fprintf(stderr, "Processing %zu blocks of %zu samples each.\n", blockCount, k);

  // Calculate the global mean
  for (size_t i = 0; i < datalen; i++) {
    globalMean += (long double)data[i];
  }
  globalMean /= (long double)datalen;  // 0 <= globalMean <= UINT32_MAX
  fprintf(stderr, "Global mean is %.22Lg.\n", globalMean);

  for (size_t i = 0; i < blockCount; i++) {
    long double localMean = 0.0L;
    long double localDelta;

    // Calculate the local mean and the necessary delta to fixup the local data.
    for (size_t j = 0; j < k; j++) {
      localMean += (long double)data[i * k + j];
    }
    localMean /= (long double)k;  // 0 <= localMean <= UINT32_MAX

    localDelta = globalMean - localMean;

    if (roundl(fabsl(localDelta)) >= 1.0L) {
      long double newLocalMean = 0.0L;
      int64_t integerAdjust = (int64_t)roundl(localDelta);

      // Rewrite the data block.
      for (size_t j = 0; j < k; j++) {
        int64_t newdata = (int64_t)data[i * k + j] + integerAdjust;
        assert(newdata >= 0);
        assert(newdata <= UINT32_MAX);
        data[i * k + j] = (uint32_t)newdata;
        newLocalMean += (long double)newdata;
      }
      newLocalMean /= (long double)k;  // 0 <= newLocalMean <= UINT32_MAX

      fprintf(stderr, "Adjusting block %zu by %ld: Block mean: %.22Lg (delta: %.22Lg) -> %.22Lg (delta: %.22Lg)\n", i, integerAdjust, localMean, fabsl(localDelta), newLocalMean, fabsl(globalMean - newLocalMean));

      assert(fabsl(globalMean - newLocalMean) <= fabsl(localDelta));
    }
  }

  if (fwrite(data, sizeof(uint32_t), datalen, stdout) != datalen) {
    if (data != NULL) free(data);
    data = NULL;
    perror("Can't write to stdout");
    exit(EX_OSERR);
  }

  if (data != NULL) free(data);
  data = NULL;
  return (0);
}
