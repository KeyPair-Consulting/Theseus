/* This file is part of the Theseus distribution.
 * Copyright 2020-2024 Joshua E. Hill <josh@keypair.us>
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

noreturn static void useageExit(void) {
  fprintf(stderr, "Usage:\n");
  fprintf(stderr, "u32-xor <file1> <file2>\n");
  fprintf(stderr, "Outputs the value of <file1> XORed with <file2>\n");
  fprintf(stderr, "The output values are send to stdout.\n");
  exit(EX_USAGE);
}

int main(int argc, char *argv[]) {
  uint32_t currentData1, currentData2;
  uint32_t outData;
  FILE *fp1, *fp2;
  bool ready1, ready2;

  if (argc != 3) useageExit();

  if ((fp1 = fopen(argv[1], "rb")) == NULL) {
    perror("Can't open first file\n");
    useageExit();
  }
  if ((fp2 = fopen(argv[2], "rb")) == NULL) {
    perror("Can't open second file.\n");
    useageExit();
  }

  currentData1 = currentData2 = 0;
  ready1 = ready2 = false;

  while ((feof(fp1) == 0) && (feof(fp2) == 0) && (ferror(fp1) == 0) && (ferror(fp2) == 0)) {
    if (!ready1 && (fread(&currentData1, sizeof(uint32_t), 1, fp1) == 1)) {
      ready1 = true;
    }
    if (!ready2 && (fread(&currentData2, sizeof(uint32_t), 1, fp2) == 1)) {
      ready2 = true;
    }

    if (ready1 && ready2) {
      outData = currentData1 ^ currentData2;
      if (fwrite(&outData, sizeof(uint32_t), 1, stdout) != 1) {
        perror("Can't write to output");
        exit(EX_OSERR);
      }
      ready1 = false;
      ready2 = false;
    }
  }

  fclose(fp1);
  fclose(fp2);

  return 0;
}
