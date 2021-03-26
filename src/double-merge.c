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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

/*Takes doubles from stdin (1 per line) and gives the pth percentile*/
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "double-merge <file1> <file2> <outfile>\n");
  fprintf(stderr, "Merges two sorted lists into a single merged list.\n");
  exit(EX_USAGE);
}

static bool verifySorted(double *data, size_t datalen) {
  double last;
  bool result = true;

  assert(datalen >= 1);
  assert(data != NULL);

  last = *data;
  data++;

  for (size_t i = 1; i < datalen; i++) {
    if (*data < last) {
      result = false;
      break;
    }

    last = *data;

    data++;
  }

  return result;
}

int main(int argc, char *argv[]) {
  size_t datalen1, datalen2;
  double *data1, *data2, *outData;
  FILE *fp;

  configVerbose = 0;
  data1 = NULL;
  data2 = NULL;

  if (argc != 4) {
    useageExit();
  }

  if ((fp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file1 for reading");
    exit(EX_OSERR);
  }
  fprintf(stderr, "Reading data set 1.\n");
  datalen1 = readdoublefile(fp, &data1);
  fclose(fp);

  if ((fp = fopen(argv[2], "rb")) == NULL) {
    perror("Can't open file2 for reading");
    exit(EX_OSERR);
  }
  fprintf(stderr, "Reading data set 2.\n");
  datalen2 = readdoublefile(fp, &data2);
  fclose(fp);

  outData = malloc(sizeof(double) * (datalen1 + datalen2));
  assert(outData);

  // Merge the data
  fprintf(stderr, "Merging the data.\n");
  mergeSortedLists(data1, datalen1, data2, datalen2, outData);
  assert(verifySorted(outData, datalen1 + datalen2));

  if ((fp = fopen(argv[3], "wb")) == NULL) {
    perror("Can't open output file for writing");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Writing the data.\n");
  fwrite(outData, sizeof(double), datalen1 + datalen2, fp);

  fclose(fp);

  free(outData);
  free(data1);
  free(data2);
  return (EX_OK);
}
