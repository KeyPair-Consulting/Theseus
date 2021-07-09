/* This file is part of the Theseus distribution.
 * Copyright 2020 Joshua E. Hill <josh@keypair.us>
 * 
 * Licensed under the 3-clause BSD license. For details, see the LICENSE file.
 *
 * Author(s)
 * Joshua E. Hill, UL VS LLC.
 * Joshua E. Hill, KeyPair Consulting, Inc.  <josh@keypair.us>
 */
#ifndef BINUTIL_H
#define BINUTIL_H

#include <stdint.h>
#include "entlib.h"

uint32_t extractbits(const uint32_t input, const uint32_t bitMask);
void extractbitsArray(const uint32_t *input, statData_t *output, const size_t datalen, const uint32_t bitMask);
uint32_t expandBits(const uint32_t input, const uint32_t bitMask);
uint32_t getActiveBits(const uint32_t *data, size_t datalen);
statData_t getActiveBitsSD(const statData_t *data, size_t datalen);
statData_t highBit(statData_t in);
statData_t lowBit(statData_t in);
uint32_t u32highBit(uint32_t in);
uint32_t u32lowBit(uint32_t in);
uint32_t reverse32(uint32_t input);
uint64_t reverse64(uint64_t input);
void reverse128(uint64_t *input);
size_t serialXOR(statData_t *data, size_t datalen, size_t compression);
#endif
