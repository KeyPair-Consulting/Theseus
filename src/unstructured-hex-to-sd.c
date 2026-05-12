/* This file is part of the Theseus distribution.
 * Copyright 2026 Joshua E. Hill <josh@keypair.us>
 *
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>
#include <sysexits.h>
#include <ctype.h>

#include "precision.h"

int main(void) {
  int res;
  char curbyte[3];
  unsigned int indata;
  uint8_t data;

  while (feof(stdin) == 0) {
    curbyte[2]=0;
    res = scanf("%c%c", curbyte, curbyte+1);

    if(res==2) {
      char lc;
      // Make this lowercase
      lc=(char)tolower(curbyte[0]);
      curbyte[0]=lc;

      lc=(char)tolower(curbyte[1]);
      curbyte[1]=lc;

      //Attempt to intreprete this as a hex byte
      res=sscanf(curbyte, "%x", &indata);
      if(res==1) {
        assert(indata <= UINT8_MAX);
        data = (uint8_t)indata;
        if (fwrite(&data, sizeof(uint8_t), 1, stdout) != 1) {
          perror("Can't write to stdout");
          exit(EX_OSERR);
        }
      } else {
        fprintf(stderr, "Can't extract byte from read hex string: %s\n", curbyte);
      }
    } else {
      if(res > 0) {
        fprintf(stderr, "Can't extract character pair from the hex string\n");
      }
    }
  }

  return (0);
}
