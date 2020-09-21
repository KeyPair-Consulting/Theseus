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
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include "entlib.h"
#include "precision.h"

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "blocks-to-sd [-l] <blocksize> <ordering>\n");
  fprintf(stderr, "Extract bits from a blocksize-sized block a byte at a time, in the specified ordering.\n");
  fprintf(stderr, "blocksize \t is the number of bytes per block\n");
  fprintf(stderr, "ordering \t is the indexing order for bytes (zero indexed decimal, separated by colons)\n");
  fprintf(stderr, "-l\t Extract bits from least to most significant within each byte.\n");
  fprintf(stderr, "The values are expected to be provided via stdin.\n");
  fprintf(stderr, "output is single bits stored in " STATDATA_STRING " sent to stdout.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "example, standard little endian 32 bit integers, data stored least to most significant:\n");
  fprintf(stderr, "\t blocks-to-sd -l 4 0:1:2:3\n");
  exit(EX_USAGE);
}

static size_t strtosize(const char *nptr, char **endptr, size_t lrange, size_t hrange) {
  long int inint;
  size_t out;

  inint = strtol(nptr, endptr, 0);
  if ((errno == ERANGE) && ((inint == LONG_MIN) || (inint == LONG_MAX))) {
    fprintf(stderr, "Can't convert integer: out of long integer range\n");
    exit(EX_USAGE);
  }

  if (inint < 0) {
    fprintf(stderr, "Can't convert integer: negative value\n");
    exit(EX_USAGE);
  }

  out = (size_t)inint;

  if ((out < lrange) || (out > hrange)) {
    fprintf(stderr, "Integer out of range\n");
    exit(EX_USAGE);
  }

  return (out);
}

int main(int argc, char *argv[]) {
  statData_t outdata;
  uint8_t bitmask;
  size_t curByteIndex;
  size_t j, k;
  size_t outputBytesPerBlock;
  bool configLTH;
  size_t blockSize;
  char *buffer;
  size_t *order;
  char *curStrLoc;
  char *nextStrLoc;
  bool foundCurrent;
  int opt;

  configLTH = false;

  while ((opt = getopt(argc, argv, "l")) != -1) {
    switch (opt) {
      case 'l':
        configLTH = true;
        break;
      default: /* ? */
        useageExit();
    }
  }

  argc -= optind;
  argv += optind;

  if (argc != 2) {
    useageExit();
  }

  blockSize = strtosize(argv[0], NULL, 0, SIZE_MAX);
  fprintf(stderr, "%zu-byte block\n", blockSize);
  if (blockSize == 0) {
    useageExit();
  }

  if ((buffer = malloc(sizeof(char) * blockSize)) == NULL) {
    perror("Can't allocate block");
    exit(EX_OSERR);
  }

  if ((order = malloc(sizeof(size_t) * blockSize)) == NULL) {
    perror("Can't allocate block order array");
    exit(EX_OSERR);
  }

  outputBytesPerBlock = 0;

  assert(argv[1] != NULL);
  curStrLoc = argv[1];

  while (*curStrLoc != '\0') {
    curByteIndex = strtosize(curStrLoc, &nextStrLoc, (size_t)0U, blockSize - 1);
    if (curStrLoc != nextStrLoc) {
      order[outputBytesPerBlock] = curByteIndex;
      outputBytesPerBlock++;
    } else {
      useageExit();
    }

    if (*nextStrLoc == ':') {
      curStrLoc = nextStrLoc + 1;
    } else {
      curStrLoc = nextStrLoc;
    }
  }

  for (k = 0; k < blockSize; k++) {
    foundCurrent = false;
    for (j = 0; j < outputBytesPerBlock; j++) {
      if (order[j] == k) {
        if (foundCurrent) {
          fprintf(stderr, "Error: Can't reference the same byte more than once\n");
          exit(EX_USAGE);
        } else {
          foundCurrent = true;
        }
      }
    }
    if (!foundCurrent) {
      fprintf(stderr, "Warning: byte %zu is dropped\n", k);
    }
  }

  while (feof(stdin) == 0) {
    if (fread(buffer, blockSize, 1, stdin) == 1) {
      for (k = 0; k < outputBytesPerBlock; k++) {
        if (configLTH) {
          bitmask = (uint8_t)0x01;
        } else {
          bitmask = (uint8_t)0x80;
        }

        for (j = 0; j < 8; j++) {
          outdata = (statData_t)(((buffer[k] & bitmask) == 0) ? 0U : 1U);
          if (fwrite(&outdata, sizeof(statData_t), 1, stdout) != 1) {
            perror("Can't write to output");
            exit(EX_OSERR);
          }

          if (configLTH) {
            bitmask = (uint8_t)((bitmask << 1) & 0xFF);
          } else {
            bitmask = bitmask >> 1;
          }
        }
      }
    }
  }

  free(buffer);
  free(order);

  return (0);
}
