# Theseus
Theseus is an implementation of the [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf) tests. 

This project is named after Claude Shannon's [mechanical maze-solving mouse](https://www.technologyreview.com/2018/12/19/138508/mighty-mouse/).

For general SP 800-90B testing topics, please see [Joshua E. Hill's SP 800-90B web page](https://www.untruth.org/~josh/sp80090b/).

## Requirements

This was written with a fairly modern Linux system running on a somewhat modern Intel CPU (it uses BMI 2, SSE 4.2 and RdRand instructions). This is intended to be compiled using either a recent version of gcc (tested using gcc version 8.4.0, or clang (tested using clang version 11.0.0). This uses the divsufsort, library, libbz2 and OpenMP, so these libraries (and their associated include files) must be installed and accessible to the compiler.

## Overview

* `src/` holds the codebase

## How to run

The tools in this package operate on symbols of type `statData_t` (`uint8_t` by default) or on `uint32_t` unless otherwise specified.

One can make all the binaries using:

	make

### Executables that Implement Portions of SP 800-90B Testing
#### `non-iid-main`
Usage:
	`non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] [-l <index>,<samples> ] <inputfile>` <br />
	or<br />
	`non-iid-main [-v] [-s] [-b <bitmask>] [-e <value>] -R <k>,<L> -f`
* An implementation of the non-IID SP 800-90B estimators.
* `<inputfile>` is presumed to consist of `statData_t` integers in machine format
* `-v`: Verbose mode (can be used up to 10 times for increased verbosity).
* `-s`:  Send verbose mode output to stdout.
* `-i`: Calculate H\_bitstring and H\_I.
* `-c`: Conditioned output, only calculate H\_bitstring.
* `-r`: Raw evaluation, do not calculate H\_bitstring.
* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
* `-b <bitmask>`: Only tests whose bits are set in `<bitmask>` are performed.
* `-g`: Use little endian conventions for creation of the bitstring used in calculation of H\_bitstring (extract data least-significant to most-significant bits).
* `-R <k>,<L>`: Randomly generate input data (`L` samples with `k` symbols) instead of reading a file.
* `-O <x>,<nu>`: Generate data from a simulated Ring Oscillator, with normalized jitter percentage of `<x>`% and normalized inter-sample expected phase change of `<nu>`. Negative `<nu>` forces random generation per-block.
* `-f`: Generate a random `<nu>` value only once, rather than on a per-evaluation-block basis.
* `-d`: Make all RNG operations deterministic.
* `-L <x>`: Perform evaluations on blocks of size `<x>`. Prevents a single assessment of the entire file (but this can be turned back on using the `-S` option).
* `-N <x>`: Perform `<x>` rounds of testing (only makes sense for randomly generated data).
* `-B <c>,<rounds>`: Perform bootstrapping for a `<c>`-confidence interval using `<rounds>`.
* `-P`: Establish an overall assessment based on a bootstrap of individual test parameters.
* `-F`: Establish an overall assessment based on a bootstrap of final assessments.
* `-S`: Establish an overall assessment using a large block assessment.

	The final assessment is the minimum of the overall assessments.


#### `restart-transpose`
Usage:
	`restart-transpose [-v] [-l <index> ] [-d <samples>,<restarts>] <inputfile>`
* Calculate the transpose of the provided restart data matrix.
* `<inputfile>` is assumed to be a sequence of `statData_t` integers
* output is sent to stdout
* `-v`: verbose mode
* `-l <index>`: Read the `<index>` substring of length `<samples * restarts>`. (default index = 0)
* `-d <samples>,<restarts>`: Perform the restart testing using the described matrix dimensions. (default is 1000x1000)

#### `restart-sanity`
Usage:
  `restart-sanity [-t <n>] [-v] [-n] [-l <index> ] [-d <samples>,<restarts>] [-c] [-i <rounds>] <H_I> <inputfile>` <br />
   or <br />
  `restart-sanity [-t <n>] [-v] [-n] [-k <m>]  [-d <samples>,<restarts>] [-c] [-i <rounds>] -r <H_I>`
* Perform the restart test for the provided restart data.
* `<inputfile>` is assumed to be a sequence of `statData_t` integers
* `<H_I>` is the assessed entropy.
*  output is sent to stdout
* `-v`: verbose mode
* `-l <index>`: Read the `<index>` substring of length `<samples * restarts>`.
* `-d <samples>,<restarts>`: Perform the restart testing using the described matrix dimensions. (default is 1000x1000).
* `-r`: Instead of doing testing on provided data use a random IID variable.
* `-k <m>`: Use an alphabet of `<m>` values (default `m`=2).
* `-n` Force counting a single fixed global symbol (Causes the test to be binomial).
* `-u`: Don't simulate the cutoff.
*  `-c <Xmaxcutoff>`: Use the provided cutoff.
* `-i <rounds>`: Use `<rounds>` simulation rounds (default is 2000000).
* `-t <n>`:  uses `<n>` computing threads (default: number of cores * 1.3).
* `-j <n>`: Each restart sanity vector is `<n>` elements long (default: min(1000,samples,restart))
* `-m <t>,<simsym>`: For simulation, Use `<t>` maximal size symbols (the residual probability is evenly distributed amongst the remaining `simsym-t` symbols).

#### `permtests`
Usage:
	`permtests [-v] [-t <n>] [-d] [-c] [-l <index>,<samples> ] <inputfile>` <br />
	or <br />
	`permtests [-v] [-b <p>] [-t <n>] [-k <m>] [-d] [-s <m>] [-c] -r`
* Perform the permutation IID tests on the provided data.
* `<inputfile>` is assumed to be a sequence of `statData_t` integers.
* `-r`: instead of doing testing on provided data use a random IID variable.
* `-d`: Make any RNG input deterministic (also force one thread).
* `-b <p>`: Set the bias to `<p>`, that is the least likely symbol has probability `k^(-p)`. Default is 1 (i.e., unbiased.)
* `-s <m>`: Use a sample set of `<m>` values (default `m`=1000000).
* `-k <k>`: Use an alphabet of `<k>` values (default `k`=2).
* `-v`: verbose mode. Repeating makes it more verbose.
* `-c`: Always complete all the tests.
* `-t <n>`: uses `<n>` computing threads (default: number of cores * 1.3).
* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.


#### `chisquare`
Usage:
	`chisquare [-v] [-l <index>,<samples> ] <inputfile>`
	or <br />
	`chisquare [-v] [-k <m>] [-s <m>] [-r]`
* Perform the chi square IID tests on the supplied data.
* `<inputfile>` is assumed to be a sequence of `statData_t` integers
* `-v`: verbose mode.
* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
* `-r`:  Instead of doing testing on provided data use a random IID variable.
* `-k <k>`:  Use an alphabet of `<k>` values (default `k`=2).
* `-s <m>`:  Use a sample set of `<m>` values (default `m`=1000000).

#### `lrs-test`
Usage:
	`lrs-test [-v] [-l <index>,<samples> ] <inputfile>` <br />
	or <br />
	`lrs-test -r [-k <m>] [-s <m>]`
* Perform the LRS IID test on the supplied data.
* `<inputfile>` is assumed to be a sequence of `statData_t`
* `-v`: Verbose mode.
* `-l <index>,<samples>`: Read the <index> substring of length <samples>.
* `-r`: instead of doing testing on provided data use a random IID variable.
* `-k <k>`:  Use an alphabet of <k> values (default `k`=2).
* `-s <m>`:  Use a sample set of <m> values (default `m`=1000000).


### Utilities for Performing Interpretation and Processing

#### `selectbits`
Usage:
	`selectbits [-l logging dir] [-v] [-t <n>] [-c] <inputfile> <outputBits>`
* Identify the bit selections that are likely to contain the most entropy, up to `<outputBits>` bits wide.
* `<inputfile>` is assumed to be a stream of `uint32_t`
* `-t <n>`: uses `<n>` computing threads (default: number of cores * 1.3).
* `-l <dir>`:  uses `<dir>` to contain the log (default: current working directory)
* `-v`: verbose mode.
* `-c`: Conservative mode (use confidence intervals with the Markov estimator).

#### `highbin`
Usage:
	`highbin <inputfile> <outputBits>`
* Attempts to bin input symbols into `2^<outputBits>` bins with equal numbers of adjacent samples.
* `<inputfile>` is assumed to be a stream of `uint32_t` ints.
* output is to stdout, and is `statData_t` ints.

#### `translate-data`
Usage:
	`translate-data [-v] <inputfile>`
* Perform an order-preserving map to arrange the input symbols to (0, ..., k-1)
* `<inputfile>` is presumed to consist of `statData_t` in machine format
* output is sent to stdout, and is `statData_t` in machine format
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-n`: No data output. Report number of symbols on stdout.

#### `u32-translate-data`
Usage:
	`u32-translate-data [-v] <inputfile>`
* Perform an order-preserving map to arrange the input symbols to (0, ..., k-1)
* `<inputfile>` is presumed to consist of `uint32_t` in machine format
* output is sent to stdout, and is `uint32_t` in machine format
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-n`: No data output. Report number of symbols on stdout.

#### `Markov`
Usage:
	`markov [-v] [-c] <inputfile>`
* Run some variant of the SP 800-90B 2016 Markov test.
* `<inputfile>` is presumed to be a stream of `statData_t` ints.
* `-v`: Verbose mode (can be used several times for increased verbosity).
* `-p <cutoff>`: The lowest acceptable probability for a symbol to be relevant.
* `-c`: Disable the creation of confidence intervals.


#### Others
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
double-minmaxdelta
double-merge

### Utilities to Produce Test Data
randomfile
simulate-osc
mementsource

### Utilities to Help Interpret Results.
percentile
mean
failrate

### Data Conversion Utilities
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
