/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <sysexits.h>

/*noreturn static void useageExit(void)
{
   fprintf(stderr, "Usage:\n");
   fprintf(stderr, "u32-xor-diff\n");
   fprintf(stderr, "Output the running XOR of adjacent values.\n");
   fprintf(stderr, "The values are expected to be provided via stdin.\n");
   exit(EX_USAGE);
}*/

int main(void) {
  uint32_t currentData;
  uint32_t priorData;
  uint32_t rawData;

  //Read in the first value
  if (fread(&priorData, sizeof(uint32_t), 1, stdin) != 1) {
    perror("Can't read initial value");
    exit(EX_OSERR);
  }

  while (feof(stdin) == 0) {
    if (fread(&currentData, sizeof(uint32_t), 1, stdin) == 1) {
      rawData = priorData ^ currentData;
      if (fwrite(&rawData, sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write to output");
        exit(EX_OSERR);
      }
      priorData = currentData;
    }
  }

  return (0);
}
