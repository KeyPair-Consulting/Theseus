# Theseus
Theseus is an implementation of the [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf) tests. 

This project is named after Claude Shannon's [mechanical maze-solving mouse](https://www.technologyreview.com/2018/12/19/138508/mighty-mouse/).

For general SP 800-90B testing topics, please see [Joshua Hill's SP 800-90B web page](https://www.untruth.org/~josh/sp80090b/).

## Requirements

This was written with a fairly modern Linux system running on a somewhat modern Intel CPU (it uses BMI 2, SSE 4.2 and RdRand instructions). This is intended to be compiled using either a recent version of gcc (tested using gcc version 8.4.0, or clang (tested using clang version 11.0.0). This uses the divsufsort, library, libbz2 and OpenMP, so these libraries (and their associated include files) must be installed and accessible to the compiler.

## Overview

* `src/` holds the codebase

## How to run

The tools in this package operate on `statData_t` (a `uint8_t` by default) or on `uint32_t` unless otherwise specified.

One can make all the binaries using:

	make

### The following executable implement portions of the SP 800-90B testing.
Usage:
    `non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] [-l <index>,<samples> ] inputfile`
    or
    `non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] -R <k>,<L> -f`

inputfile is presumed to consist of `statData_t` integers in machine format
* `-v`: Verbose mode (can be used up to 10 times for increased verbosity).
* `-s`:  Send verbose mode output to stdout.
* `-i`: Calculate H\_bitstring and H\_I.
* `-c`: Conditioned output, only calculate H\_bitstring.
* `-r`: Raw evaluation, do not calculate H\_bitstring.
* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
* `-b <bitmask>`: Only tests whose bits are set in `<bitmask>` are performed.
* `-g`: Use little endian conventions for creation of the bitstring used in calculation of H\_bitstring (extract data least-significant to most-significant bits).
* `-R <k>,<L>`: Randomly generate input data (L samples with k symbols) instead of reading a file.
* `-O <x>,<nu>`: Generate RO bitwise data with normalized jitter percentage of `<x>`% and normalized inter-sample expected phase change of `<nu>`. Negative nu forces random generation per-block.
* `-f`: Generate a random `<nu>` value only once, rather than on a per-evaluation-block basis.
* `-d`: Make all RNG operations deterministic.
* `-L <x>`: Perform evaluations on blocks of size x. Prevents a single assessment of the entire file (but this can be turned back on using the \"-S\" option).
* `-N <x>`: Perform `<x>` rounds of testing (only makes sense for randomly generated data).
* `-B <c>,<rounds>`: Perform bootstrapping for a `<c>`-confidence interval using `<rounds>`.
* `-P`: Establish an overall assessment based on bootstrap of individual test parameters.
* `-F`: Establish an overall assessment based on bootstrap of final assessments.
* `-S`: Establish an overall assessment using a large block assessment.

The final assessment is the minimum of the overall assessments.


lrs-test
permtests
chisquare
markov
restart-transpose
restart-sanity

### This package contains some utilities that are useful sources of test data.
randomfile
simulate-osc
mementsource

### This package contains a number of tools that are used to interpret results.
percentile
mean
failrate
double-minmaxdelta
double-merge

### This package also contains a number of utilities for performing certain styles of data interpretation and processing:
selectbits
highbin
translate-data
u32-translate-data

u32-selectdata
u32-selectrange
u32-xor-diff
u128-bit-select
u32-discard-fixed-bits
extractbits
u32-anddata
double-sort
u32-bit-select
u32-xor
hweight
bits-in-use
u32-bit-permute
discard-fixed-bits
downsample
u128-discard-fixed-bits
u32-gcd
u32-manbin
u32-regress-to-mean

### This package also contains a number of utilities for converting data between various formats:
u64-to-u32
u64-counter-raw
u8-to-sd
u16-to-u32
u64-to-ascii
u32-to-sd
u16-to-sd
u64-jent-to-delta
u32-to-categorical
dec-to-u32
u32-expand-bitwidth
u32-to-ascii
sd-to-hex
u8-to-u32
hex-to-u32
blocks-to-sd
u32-counter-endian
u32-delta
u32-counter-bitwidth
u32-counter-raw
u64-counter-endian

## Make

Several `Makefile`s are provided; these are useful in various contexts which are hopefully reasonably clear from the name.

## More Information

For more information on the estimation methods, see [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf).
