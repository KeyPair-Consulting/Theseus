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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

#include "binio.h"
#include "entlib.h"
#include "globals-inst.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-to-sd <inputfile>\n");
  fprintf(stderr, "inputfile is assumed to be a stream of uint32_ts\n");
  fprintf(stderr, "output sent to stdout is a stream of " STATDATA_STRING " integers\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  FILE *infp;
  size_t datalen;
  uint32_t *data = NULL;
  statData_t *dataout = NULL;
  size_t i;

  if (argc != 2) {
    useageExit();
  }

  if ((infp = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open file");
    exit(EX_NOINPUT);
  }

  if((datalen = readuint32file(infp, &data))<1) {
    perror("Data file is empty");
    exit(EX_DATAERR);
  }
  assert(data != NULL);

  if ((dataout = malloc(sizeof(statData_t) * datalen)) == NULL) {
    perror("Can't allocate output array");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Read in %zu uint32_ts\n", datalen);
  if (fclose(infp) != 0) {
    perror("Can't close intput file");
    exit(EX_OSERR);
  }

  fprintf(stderr, "Outputting data\n");
  for (i = 0; i < datalen; i++) {
    if (data[i] > STATDATA_MAX) {
      fprintf(stderr, "Value out of range\n");
      free(data);
      free(dataout);
      exit(EX_DATAERR);
    } else {
      dataout[i] = (statData_t)data[i];
    }
  }

  if (fwrite(dataout, sizeof(statData_t), datalen, stdout) != datalen) {
    perror("Can't write output to stdout");
    exit(EX_OSERR);
  }

  free(data);
  free(dataout);

  return EX_OK;
}
