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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#if defined(__x86_64) || defined(__x86_64__)
#include <x86intrin.h>
#endif

#include "binio.h"
#include "entlib.h"
#include "globals.h"
#include "precision.h"

static size_t getfilesize(FILE *input) {
  long size, savedLoc;

  if ((savedLoc = ftell(input)) < 0) {
    perror("Can't find current location.");
    exit(EX_OSERR);
  }

  if (fseek(input, 0, SEEK_END) < 0) {
    perror("Can't seek to end location.");
    exit(EX_OSERR);
  }

  if ((size = ftell(input)) < 0) {
    perror("Can't find end location.");
    exit(EX_OSERR);
  }

  fseek(input, savedLoc, SEEK_SET);
  if (fseek(input, savedLoc, SEEK_SET) < 0) {
    perror("Can't seek to saved location.");
    exit(EX_OSERR);
  }

  return ((size_t)size);
}

size_t readdoublefile(FILE *input, double **buffer) {
  size_t fileSize;
  size_t readdoubles = 0;
  size_t filedoubles;
  size_t rem;

  assert(buffer != NULL);

  fileSize = getfilesize(input);
  rem = fileSize % sizeof(double);

  if (rem != 0) {
    fprintf(stderr, "Extra bytes at the end of the file\n");
    fileSize -= rem;
  }

  filedoubles = fileSize / sizeof(double);

  if ((*buffer = malloc(filedoubles * sizeof(double))) == NULL) {
    perror("Can't get memory");
    exit(EX_OSERR);
  }

  while ((feof(input) == 0) && (readdoubles < filedoubles)) {
    readdoubles += fread((*buffer) + readdoubles, sizeof(double), filedoubles - readdoubles, input);

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }
  return readdoubles;
}

size_t readuint64file(FILE *input, uint64_t **buffer) {
  size_t fileSize;
  size_t readints = 0;
  size_t fileints;
  size_t rem;

  assert(buffer != NULL);

  fileSize = getfilesize(input);
  rem = fileSize % sizeof(uint64_t);

  if (rem != 0) {
    fprintf(stderr, "Extra bytes at the end of the file\n");
    fileSize -= rem;
  }

  fileints = fileSize / sizeof(uint64_t);

  if ((*buffer = malloc(fileints * sizeof(uint64_t))) == NULL) {
    perror("Can't get memory");
    exit(EX_OSERR);
  }

  while ((feof(input) == 0) && (readints < fileints)) {
    readints += fread((*buffer) + readints, sizeof(uint64_t), fileints - readints, input);

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }
  return readints;
}

size_t readuint32file(FILE *input, uint32_t **buffer) {
  size_t fileSize;
  size_t readints = 0;
  size_t fileints;
  size_t rem;

  assert(buffer != NULL);

  fileSize = getfilesize(input);
  rem = fileSize % sizeof(uint32_t);

  if (rem != 0) {
    fprintf(stderr, "Extra bytes at the end of the file\n");
    fileSize -= rem;
  }

  fileints = fileSize / sizeof(uint32_t);

  if ((*buffer = malloc(fileints * sizeof(uint32_t))) == NULL) {
    perror("Can't get memory");
    exit(EX_OSERR);
  }

  while ((feof(input) == 0) && (readints < fileints)) {
    readints += fread((*buffer) + readints, sizeof(uint32_t), fileints - readints, input);

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }

  return readints;
}

size_t readuintfile(FILE *input, statData_t **buffer) {
  size_t fileSize;
  size_t readints = 0;
  size_t fileints;
  size_t rem;

  assert(buffer != NULL);

  fileSize = getfilesize(input);
  rem = fileSize % sizeof(statData_t);

  if (rem != 0) {
    fprintf(stderr, "Extra bytes at the end of the file\n");
    fileSize -= rem;
  }

  fileints = fileSize / sizeof(statData_t);

  if ((*buffer = malloc(fileints * sizeof(statData_t))) == NULL) {
    perror("Can't get memory");
    exit(EX_OSERR);
  }

  while ((feof(input) == 0) && (readints < fileints)) {
    readints += fread((*buffer) + readints, sizeof(statData_t), fileints - readints, input);

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }
  return readints;
}

size_t readuintfileloc(FILE *input, statData_t **buffer, size_t subsetIndex, size_t subsetSize) {
  size_t readints = 0;
  long int loc;

  assert(buffer != NULL);

  if (subsetSize == 0) {
    return readuintfile(input, buffer);
  }

  loc = (long int)(subsetIndex * subsetSize * sizeof(statData_t));
  if (fseek(input, loc, SEEK_SET) != 0) {
    perror("Cannot seek to desired location");
    exit(EX_DATAERR);
  }

  if (((*buffer) = (statData_t *)malloc(sizeof(statData_t) * subsetSize)) == NULL) {
    perror("Cannot allocate new memory block");
    exit(EX_OSERR);
  }

  readints = 0;
  while ((feof(input) == 0) && (subsetSize > readints)) {
    readints += fread((*buffer) + readints, sizeof(statData_t), subsetSize - readints, input);
  }

  return readints;
}

/*Merge two sorted lists, and place the result into out.*/
void mergeSortedLists(const double *in1, size_t len1, const double *in2, size_t len2, double *out) {
  assert(in1 != NULL);
  assert(in2 != NULL);

  while ((len1 > 0) && (len2 > 0)) {
    if (*in1 <= *in2) {
      *out = *in1;
      in1++;
      len1--;
    } else {
      *out = *in2;
      in2++;
      len2--;
    }
    out++;
  }

  if (len1 > 0) {
    memcpy(out, in1, sizeof(double) * len1);
  } else if (len2 > 0) {
    memcpy(out, in2, sizeof(double) * len2);
  }
}

size_t readasciidoubles(FILE *input, double **buffer) {
  double *newbuffer;
  long int scdata;
  size_t curbuflen = 0;
  size_t pagesize;
  size_t readdoubles = 0;
  char curline[4096];
  double indouble;
  char *afterDouble;

  assert(buffer != NULL);

  scdata = sysconf(_SC_PAGESIZE);
  assert(scdata > 0);
  pagesize = (size_t)scdata;

  while (feof(input) == 0) {
    if (((readdoubles + 1) * sizeof(double)) > curbuflen) {
      if ((newbuffer = realloc(*buffer, curbuflen + pagesize)) == NULL) {
        perror("Cannot allocate new memory block");
        exit(EX_OSERR);
      } else {
        *buffer = newbuffer;
        curbuflen += pagesize;
      }
    }

    if (fgets(curline, sizeof(curline), input) != NULL) {
      indouble = strtod(curline, &afterDouble);
      if (((*afterDouble != '\r') && (*afterDouble != '\n') && (*afterDouble != '\0')) || (indouble >= HUGE_VAL) || (indouble <= -HUGE_VAL) || (errno == ERANGE)) {
        fprintf(stderr, "data error\n");
        exit(EX_DATAERR);
      }
      (*buffer)[readdoubles] = indouble;
      readdoubles++;
    }

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }

  return readdoubles;
}

size_t readasciidoublepoints(FILE *input, double **buffer) {
  double *newbuffer;
  long int scdata;
  size_t curbuflen = 0;
  size_t pagesize;
  size_t readdoubles = 0;
  char curline[4096];
  char *curloc;
  double indouble;
  char *afterDouble;

  assert(buffer != NULL);

  scdata = sysconf(_SC_PAGESIZE);
  assert(scdata > 0);
  pagesize = (size_t)scdata;

  while (feof(input) == 0) {
    if (((readdoubles + 2) * sizeof(double)) > curbuflen) {
      if ((newbuffer = realloc(*buffer, curbuflen + pagesize)) == NULL) {
        perror("Cannot allocate new memory block");
        exit(EX_OSERR);
      } else {
        *buffer = newbuffer;
        curbuflen += pagesize;
      }
    }

    if (fgets(curline, sizeof(curline), input) != NULL) {
      curloc = curline;
      if ((*curloc == '(') || (*curloc == '[') || (*curloc == '{')) curloc++;
      indouble = strtod(curloc, &afterDouble);
      if ((*afterDouble != ',') || (errno == ERANGE)) {
        fprintf(stderr, "First place data error\n");
        exit(EX_DATAERR);
      }
      (*buffer)[readdoubles++] = indouble;

      curloc = afterDouble + 1;
      indouble = strtod(curloc, &afterDouble);

      if (((*afterDouble != '\r') && (*afterDouble != '\n') && (*afterDouble != '\0') && (*afterDouble != ')') && (*afterDouble != ']') && (*afterDouble != '}')) || (errno == ERANGE) || !isfinite(indouble)) {
        fprintf(stderr, "Second place data error: \"%c\"\n", *afterDouble);
        exit(EX_DATAERR);
      }
      (*buffer)[readdoubles++] = indouble;
    }

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }

  return readdoubles / 2;
}

size_t readasciiuint64s(FILE *input, uint64_t **buffer) {
  uint64_t *newbuffer;
  long int scdata;
  size_t curbuflen = 0;
  size_t pagesize;
  size_t readuints = 0;
  char curline[4096];
  unsigned long long inInt;
  char *afterInt;

  assert(buffer != NULL);

  scdata = sysconf(_SC_PAGESIZE);
  assert(scdata > 0);
  pagesize = (size_t)scdata;

  while (feof(input) == 0) {
    if (((readuints + 1) * sizeof(uint64_t)) > curbuflen) {
      if ((newbuffer = realloc(*buffer, curbuflen + pagesize)) == NULL) {
        perror("Cannot allocate new memory block");
        exit(EX_OSERR);
      } else {
        *buffer = newbuffer;
        curbuflen += pagesize;
      }
    }

    if (fgets(curline, sizeof(curline), input) != NULL) {
      inInt = strtoull(curline, &afterInt, 0);

      if (((*afterInt != '\r') && (*afterInt != '\n') && (*afterInt != '\0')) || ((inInt == ULLONG_MAX) && (errno == ERANGE))) {
        fprintf(stderr, "data error\n");
        exit(EX_DATAERR);
      }
      (*buffer)[readuints] = inInt;
      readuints++;
    }

    if (ferror(input) != 0) {
      perror("Error reading input file");
      exit(EX_OSERR);
    }
  }

  return readuints;
}
