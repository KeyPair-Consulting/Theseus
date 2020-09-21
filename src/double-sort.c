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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>
#include <unistd.h>

#include "binio.h"
#include "globals-inst.h"
#include "precision.h"

/*Takes doubles and sorts them, outputting them to stdout.*/ 
noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "double-sort <filename>\n");
  fprintf(stderr, "Takes doubles from the file and sorts them.\n");
  exit(EX_USAGE);
}

//No fancyness is needed here.
static int doublecompare(const void *in1, const void *in2) {
  double left, right;

  assert(in1 != NULL);
  assert(in2 != NULL);

  left = *((const double *)in1);
  right = *((const double *)in2);

  assert(!isnan(left) && !isnan(right));

  if (left < right) {
    return (-1);
  } else if (left > right) {
    return (1);
  } else {
    return (0);
  }
}

int main(int argc, char *argv[]) {
  size_t datalen;
  double *data;
  FILE *fp;

  configVerbose = 0;
  data = NULL;

  if (argc != 2) {
    useageExit();
  }

  if ((fp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file for reading");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Reading the data.\n");
  if((datalen = readdoublefile(fp, &data)) < 1) {
    perror("File is empty");
    exit(EX_DATAERR);
  }

  fclose(fp);

  if (datalen == 0) {
    useageExit();
  }

  assert(data != NULL);

  // Sort the data
  fprintf(stderr, "Sorting the data.\n");
  qsort(data, datalen, sizeof(double), doublecompare);

  fprintf(stderr, "Writing the data.\n");
  while((datalen > 0) && (ferror(stdout)!=0)) {
	size_t written;

  	written = fwrite(data, sizeof(double), datalen, stdout);
	data += written;
	datalen -= written;
  }

  if(ferror(stdout) != 0) {
    perror("Can't write sorted data");
    exit(EX_DATAERR);
  }
	
  free(data);
  return (EX_OK);
}
