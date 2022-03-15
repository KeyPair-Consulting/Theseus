# Theseus
Theseus is an implementation of the [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf) tests. 

This project is named after Claude Shannon's [mechanical maze-solving mouse](https://www.technologyreview.com/2018/12/19/138508/mighty-mouse/).

For general SP 800-90B testing topics, please see [Joshua E. Hill's SP 800-90B web page](https://www.untruth.org/~josh/sp80090b/).

## Requirements

This was written with a fairly modern Linux system running on a somewhat modern Intel CPU (it uses BMI 2, SSE 4.2 and RdRand instructions). This is intended to be compiled using either a recent version of gcc (tested using gcc version 10.3.0, or clang (tested using clang version 12.0.1). This uses divsufsort, libbz2 and OpenMP, so these libraries (and their associated include files) must be installed and accessible to the compiler.

## Folder Structure

The folder structure is as follows:
* `docs/`: Contains documentation/examples for each function.
* `ex/`: Contains data files used in examples.
* `src/`: Contains the codebase.

## How to Run

The tools in this package operate on symbols of type `statData_t` (`uint8_t` by default) or on `uint32_t` unless otherwise specified.

One can make all the binaries using:

	make

Several `Makefile`s are provided; these are useful in various contexts which are hopefully reasonably clear from the name.

## Overview

Below is a summary of available Theseus functions.  Detailed documentation for each function can be found in the `docs/` folder and links are provided.

### Executables that Implement Portions of SP 800-90B Testing

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[non-iid-main](./docs/SP800-90B_TESTING.md#non-iid-main) | An implementation of the non-IID SP 800-90B estimators.|
|[restart-transpose](./docs/SP800-90B_TESTING.md#restart-transpose) | Calculate the transpose of the provided restart data matrix.|
|[restart-sanity](./docs/SP800-90B_TESTING.md#restart-sanity) | Perform the restart test for the provided restart data.|
|[permtests](./docs/SP800-90B_TESTING.md#permtests) | Perform the permutation IID tests on the provided data.|
|[chisquare](./docs/SP800-90B_TESTING.md#chisquare) | Perform the chi square IID tests on the supplied data.|
|[lrs-test](./docs/SP800-90B_TESTING.md#lrs-test) | Perform the LRS IID test on the supplied data.|
|[Markov](./docs/SP800-90B_TESTING.md#Markov) | Run some variant of the SP 800-90B 2016 Markov test.|

### H_submitter Production Utilities

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[ro-model](./docs/HSUBMITTER_PRODUCTION_UTILITIES.md#ro-model) | Produce a min entropy estimate using the selected stochastic model.|
|[linear-interpolate](./docs/HSUBMITTER_PRODUCTION_UTILITIES.md#linear-interpolate) | Takes a set of points (`x_1`, `y_1`), ... (`x_n`, `y_n`) and treats the points as a relation. This tool can be used to infer parameters from the statistical results.|

### Test Data Production Utilities

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[randomfile](./docs/TEST_DATA_PRODUCTION_UTILITIES.md#randomfile) | Create a random data file for use in testing.|
|[simulate-osc](./docs/TEST_DATA_PRODUCTION_UTILITIES.md#simulate-osc) | Create a random data file for use in testing that simulates a ring oscillator.|

### Result Interpretation Utilities

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[percentile](./docs/RESULT_INTERPRETATION_UTILITIES.md#percentile) | Takes a set of human-readable doubles and provides the pth percentile.  Percentile is estimated using the recommended NIST method [Hyndman and Fan's R6](http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm).|
|[mean](./docs/RESULT_INTERPRETATION_UTILITIES.md#mean) | Takes a set of human-readable doubles and provides the mean.|
|[failrate](./docs/RESULT_INTERPRETATION_UTILITIES.md#failrate) |  Takes a set of human-readable doubles and provides the proportion that are less than or equal to `<p>` or greater than or equal to `<q>`.  This is useful to characterize false positive rates for statistical tests with inclusive failure bounds.|

### Data Conversion Utilities

#### Functions for word size and base input conversion:

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[u8-to-u32](./docs/DATA_CONVERSION_UTILITIES.md#u8-to-u32) | Converts provided binary data from type uint8_t to type uint32_t.|
|[u16-to-u32](./docs/DATA_CONVERSION_UTILITIES.md#u16-to-u32) | Converts provided binary data from type uint16_t to type uint32_t.|
|[u64-to-u32](./docs/DATA_CONVERSION_UTILITIES.md#u64-to-u32) | Converts provided binary data from type uint64_t to type uint32_t.|
|[dec-to-u32](./docs/DATA_CONVERSION_UTILITIES.md#dec-to-u32) | Converts provided human-readable decimal values to binary data.  (Note this is the opposite of u32-to-ascii.)|
|[hex-to-u32](./docs/DATA_CONVERSION_UTILITIES.md#hex-to-u32) | Converts provided human-readable hexidecimal values to binary data.|

#### Functions related to deltas, counters, endianness, and bit width:

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[u32-delta](./docs/DATA_CONVERSION_UTILITIES.md#u32-delta) | Extracts deltas and then translates the result to positive values.|
|[u64-jent-to-delta](./docs/DATA_CONVERSION_UTILITIES.md#u64-jent-to-delta) | Converts provided binary data from uint64_t deltas in jent format (JEnt version 3.0.1 and earlier) to uint64_t deltas in nanoseconds format.  Also guesses native byte order and swaps if necessary.  Jent format expects the upper 32 bits to contain seconds and the lower 32 bits to contain nanoseconds.|
|[u32-counter-raw](./docs/DATA_CONVERSION_UTILITIES.md#u32-counter-raw) | Extracts deltas treated as 32-bit unsigned counters (they may roll over).|
|[u64-counter-raw](./docs/DATA_CONVERSION_UTILITIES.md#u64-counter-raw) | Extracts deltas treated as 64-bit unsigned counters (they may roll over).|
|[u32-counter-endian](./docs/DATA_CONVERSION_UTILITIES.md#u32-counter-endian) | Trys to guess counter endianness and translates to the local platform.|
|[u64-counter-endian](./docs/DATA_CONVERSION_UTILITIES.md#u64-counter-endian) | Trys to guess counter endianness and translates to the local platform.|
|[u64-change-endianness](./docs/DATA_CONVERSION_UTILITIES.md#u64-change-endianness) | Changes between big and little endian byte-ordering conventions.|
|[u32-counter-bitwidth](./docs/DATA_CONVERSION_UTILITIES.md#u32-counter-bitwidth) | Extracts deltas under the assumption that the data is an increasing counter of some inferred bitwidth.|
|[u32-expand-bitwidth](./docs/DATA_CONVERSION_UTILITIES.md#u32-expand-bitwidth) | Extracts inferred values under the assumption that the data is a truncation of some sampled value, whose bitwidth is inferred.|

#### Functions for converting to type statData_t for statistical analysis:

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[u8-to-sd](./docs/DATA_CONVERSION_UTILITIES.md#u8-to-sd) | Converts provided binary data from type uint8_t to type statData_t.|
|[u16-to-sdbin](./docs/DATA_CONVERSION_UTILITIES.md#u16-to-sdbin) | Converts provided binary data from type uint16_t to type statData_t by expanding packed bits.|
|[u32-to-sd](./docs/DATA_CONVERSION_UTILITIES.md#u32-to-sd) | Converts provided binary data from type uint32_t to type statData_t.|
|[blocks-to-sdbin](./docs/DATA_CONVERSION_UTILITIES.md#blocks-to-sdbin) | Extracts bits from a blocksize-sized block one byte at a time, in the specified byte ordering.|

#### Functions for output conversion (for histograms, human readability, categorization, etc.):

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[sd-to-hex](./docs/DATA_CONVERSION_UTILITIES.md#sd-to-hex) | Converts provided binary data to human-readable hexidecimal values.|
|[u32-to-ascii](./docs/DATA_CONVERSION_UTILITIES.md#u32-to-ascii) | Converts provided binary data to human-readable decimal values.  (Note this is the opposite of dec-to-u32.)|
|[u64-to-ascii](./docs/DATA_CONVERSION_UTILITIES.md#u64-to-ascii) | Converts provided binary data to human-readable decimal values.|
|[u32-to-categorical](./docs/DATA_CONVERSION_UTILITIES.md#u32-to-categorical) | Produces categorical summary of provided binary data.|

### Other Data Utilities

| Function Name | Description                                                        |
|:--------------|:-------------------------------------------------------------------|
|[bits-in-use](./docs/OTHER_DATA_UTILITIES.md#bits-in-use) | Determines the number of bits required to represent the given data after removing stuck and superfluous bits.|
|[discard-fixed-bits](./docs/OTHER_DATA_UTILITIES.md#discard-fixed-bits) | Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.|
|[double-merge](./docs/OTHER_DATA_UTILITIES.md#double-merge) | Merges two sorted lists of doubles into a single merged sorted list of doubles.|
|[double-minmaxdelta](./docs/OTHER_DATA_UTILITIES.md#double-minmaxdelta) | Takes a set of human-readable doubles and provides the mean.|
|[double-sort](./docs/OTHER_DATA_UTILITIES.md#double-sort) | Takes doubles from the file and sorts them.|
|[downsample](./docs/OTHER_DATA_UTILITIES.md#downsample) | Groups data by index into modular classes mod `<rate>` evenly into the `<block size>`.|
|[extractbits](./docs/OTHER_DATA_UTILITIES.md#extractbits) | Takes the given binary data and extracts bits with `<bitmask>`.|
|[highbin](./docs/OTHER_DATA_UTILITIES.md#highbin) | Attempts to bin input symbols into `2^<outputBits>` bins with equal numbers of adjacent samples.|
|[hweight](./docs/OTHER_DATA_UTILITIES.md#hweight) | Calculates the Hamming weight of `<bitmask>`.  As an example, the bit string 11101000 has a Hamming weight of 4.|
|[selectbits](./docs/OTHER_DATA_UTILITIES.md#selectbits) | Identify the bit selections that are likely to contain the most entropy, up to `<outputBits>` bits wide.|
|[translate-data](./docs/OTHER_DATA_UTILITIES.md#translate-data) | Perform an order-preserving map to arrange the input symbols to (0, ..., k-1).|
|[u128-bit-select](./docs/OTHER_DATA_UTILITIES.md#u128-bit-select) | Selects and returns the value in the given bit position (0 is the LSB, 127 is the MSB, little endian is assumed).|
|[u128-discard-fixed-bits](./docs/OTHER_DATA_UTILITIES.md#u128-discard-fixed-bits) | Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.|
|[u32-anddata](./docs/OTHER_DATA_UTILITIES.md#u32-anddata) | Takes the given binary data and bitwise ANDs each symbol with `<bitmask>`.|
|[u32-bit-permute](./docs/OTHER_DATA_UTILITIES.md#u32-bit-permute) | Permute bits within the given binary data as specified in `<bit specification>`.  Bit ordering is specified in the LSB 0 format (i.e., bit 0 is the LSB and bit 31 is the MSB).|
|[u32-bit-select](./docs/OTHER_DATA_UTILITIES.md#u32-bit-select) | Selects and returns the value in the given bit position (0 is the LSB, 31 is the MSB).|
|[u32-discard-fixed-bits](./docs/OTHER_DATA_UTILITIES.md#u32-discard-fixed-bits) | Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.|
|[u32-gcd](./docs/OTHER_DATA_UTILITIES.md#u32-gcd) | Finds common divisors and removes these factors from the given binary data.|
|[u32-manbin](./docs/OTHER_DATA_UTILITIES.md#u32-manbin) | Assign given binary data to one of the n bin numbers (0, ..., n-1).|
|[u32-regress-to-mean](./docs/OTHER_DATA_UTILITIES.md#u32-regress-to-mean) | Calculate the mean, force each `k`-block to have this mean, and then round the resulting values.|
|[u32-selectdata](./docs/OTHER_DATA_UTILITIES.md#u32-selectdata) | Attempt to keep the percentages noted in the provided binary data.|
|[u32-selectrange](./docs/OTHER_DATA_UTILITIES.md#u32-selectrange) | Extracts all values from the given binary data between a specified `low` and `high` (inclusive).|
|[u32-translate-data](./docs/OTHER_DATA_UTILITIES.md#u32-translate-data) | Perform an order-preserving map to arrange the input symbols to (0, ..., k-1).|
|[u32-xor-diff](./docs/OTHER_DATA_UTILITIES.md#u32-xor-diff) | Produces the running XOR of adjacent values in provided binary data.|

## More Information

For more information on the estimation methods, see [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf).
