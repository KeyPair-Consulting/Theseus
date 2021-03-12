# Theseus
Theseus is an implementation of the [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf) tests. 

This project is named after Claude Shannon's [mechanical maze-solving mouse](https://www.technologyreview.com/2018/12/19/138508/mighty-mouse/).

For general SP 800-90B testing topics, please see [Joshua E. Hill's SP 800-90B web page](https://www.untruth.org/~josh/sp80090b/).

## Requirements

This was written with a fairly modern Linux system running on a somewhat modern Intel CPU (it uses BMI 2, SSE 4.2 and RdRand instructions). This is intended to be compiled using either a recent version of gcc (tested using gcc version 10.1.0, or clang (tested using clang version 11.1.0). This uses the divsufsort, library, libbz2 and OpenMP, so these libraries (and their associated include files) must be installed and accessible to the compiler.

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
* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
* `-r`: instead of doing testing on provided data use a random IID variable.
* `-k <k>`:  Use an alphabet of `<k>` values (default `k`=2).
* `-s <m>`:  Use a sample set of `<m>` values (default `m`=1000000).


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


### Others
#### `u32-selectdata`
Usage:
	`u32-selectdata inputfile trimLowPercent trimHighPercent`
* Attempt to keep the percentages noted.
* inputfile is assumed to be a stream of uint32\_ts
* output is to stdout, and is u32 ints

#### `u32-selectrange`
Usage:
	`u32-selectrange inputfile low high`
* Extracts all values between low and high (inclusive).
* inputfile is assumed to be a stream of uint32\_ts
* output is to stdout, and is u32 ints

#### `u32-xor-diff`
Usage:
	`u32-xor-diff`
* Output the running XOR of adjacent values. 
* The values are expected to be provided via stdin.

#### `u128-bit-select`
Usage:
	`bit-select-u128 (bit)`
* The 128-bit values are expected to be provided via stdin.
* output is uint8_t via stdout.

#### `u32-discard-fixed-bits`
Usage:
	`u32-discard-fixed-bits <inputfile>`
* inputfile is assumed to be a stream of uint32\_ts
* output sent to stdout are uint32\_t integers, with all fixed bits removed and the non-fixed bits moved to the LSB of the output.

#### `extractbits`
Usage:
	`extractbits <inputfile> <bitmask>`
* inputfile is assumed to be a stream of uint32\_ts
* output is sent to stdout is in uint8\_t format

#### `u32-anddata`
Usage:
	`u32-anddata [inputfile] <bitmask>`
* Takes the uint32\_t symbols in (machine format) and bitwise ANDs each symbol with `<bitmask>`.
* inputfile is assumed to be a stream of uint32\_ts
* Outputs uint32\_ts to stdout
* The result is the data bitwise anded with the provided bitmask output to stdout

#### `double-sort`
Usage:
	`double-sort <filename>`
* Takes doubles from the file and sorts them.

#### `u32-bit-select`
Usage:
	`u32-bit-select [-r] (bit)`
* `-r`: Reverse the endianness of the u32 inputs before selecting the bit.
* Outputs only the selected bit (0 is the lsb, 31 is msb).
* The 32-bit values are expected to be provided via stdin.
* output is uint8\_t via stdout.

#### `hweight`
Usage:
	`hweight bitmask`
* output is the hamming weight of the bitmask

#### `bits-in-use`
Usage:
	`bits-in-use <inputfile>`
* inputfile is assumed to consist of uint32\_ts
* outputs the number of bits required to represent data after removing stuck and superfluous bits.

#### `u32-bit-permute`
Usage:
	`u32-bit-permute [-r] <bit specification>`
* `-r`: Reverse the endianness of the u32 inputs before permuting.
* Bit ordering is in the LSB 0 format (that is, bit 0 is the LSB, bit 31 is the MSB)
* Ordering of the bit specification is left to right, MSB to LSB, so the specification "31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0" is the identity permutation.
* If fewer than 32 output bits are within the specification, the unspecified high order bits are set to 0
* Each bit position can be present at most once.
* The 32-bit values are expected to be provided via stdin.
* output is u32 values via stdout in machine native format.

#### `discard-fixed-bits`
Usage:
	`discard-fixed-bits inputfile`
* inputfile is assumed to be a stream of uint8\_t
* output sent to stdout are uint8\_t, with all fixed bits removed and the non-fixed bits moved to the lsb of the output.

#### `downsample`
Usage:
	`downsample [-b <block size>] <rate> <data file>`
* Groups data by index into modular classes mod `<rate>` evenly into the block size.
* `<rate>`: Number of input samples per output samples
* `-b`: Samples per output block (default 1000000)
* The uint8\_t values are output via stdout.

#### `u128-discard-fixed-bits`
Usage:
	`discard-fixed-bits-u128 inputfile output`
* inputfile is assumed to be a stream of uint128\_t
* output sent to stdout are uint32\_tegers, with all fixed bits removed and the non-fixed bits moved to the LSB of the output.

#### `u32-gcd`
Usage:
	`u32-gcd <filename>`
* Find common divisors, and remove these factors.
* The values are output to stdout.

#### `u32-manbin`
Usage:
	`u32-manbin inputfile cutoff_1 ... cutoff_{n-1}`
* convert data to one of the n bin numbers (0, ..., n-1).
* The cutoffs specify the first value in the next bin (so the first bin is `[0, cutoff_1)`, the second bin is `[cutoff_1, cutoff_2)`, the last bin is `[cutoff_{n-1}, UINT32_MAX ]` etc.)
* inputfile is assumed to be a stream of uint32\_ts
* output is to stdout, and is uint8\_t values

#### `u32-regress-to-mean`
Usage:
	`u32-regress-to-mean <filename> <k>`
* Calculate the mean, and then force each `k`-block to have this mean, and then round the resulting values.

#### `double-minmaxdelta`
Usage:
	`double-minmaxdelta [-v] [-0] [filename]`
* Takes doubles from the file (if provided) or stdin (by default) (1 per line in ascii mode) and gives the mean.
* Verbose mode (can be used up to 3 times for increased verbosity).
* `-0`: Read in doubles in machine-specific format.

#### `double-merge`
Usage:
	`double-merge <file1> <file2> <outfile>`
* Merges two sorted lists into a single merged sorted list.

### Utilities to help produce H\_submitter

#### `ro-model`
Usage:
	`ro-model [-v] [-J] [-B] [-S] [-K] [-W] [-g gaussian-prop] <sigma>` <br />
* Produce a min entropy estimate using the selected stochastic model.
* Sigma is the observed normalized (period length) jitter standard deviation (expressed as a proportion of the ring oscillator period).
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-J`: Use the Jackson stochastic model.
* `-B`: Use the BLMT stochastic model.
* `-S`: Use the Saarinen stochastic model.
* `-K`: Use the Killmann-Schindler stochastic model.
* `-W`: Use the worst-case Killmann-Schindler stochastic model.
* `-g <r>`: Operate under the assumption that the unpredictable portion of the observed jitter is sigma*r (so reduce the observed jitter by the factor r).

Some example results are contained within the following graphs: <br />
![Ring Oscillator Stochastic Model Results](ro-model.png)

![Zoomed In Ring Oscillator Stochastic Model Results](ro-model-zoom.png)

Note that the Killmann-Schindler stochastic model and the Saarinen stochastic model produce effectively equivalent results.

#### `linear-interpolate`
Usage:
	`linear-interpolate [-v] [-i] [-x] <value>`
* Takes a set of points (`x_1`, `y_1`), ... (`x_n`, `y_n`), one point per line, from stdin and treats the points as a relation.
* The relation is then forced to be functional by discarding point; by default, the point with the lowest value (y-value) is retained for each distinct argument (x-value), unless otherwise directed.
* The points in the resulting function are then used to define and extension function f: `[x_1, x_n] -> R`, where the values are established through linear interpolation.
* The reference data may include the arguments (x-values) INFINITY (or INF) and -INFINITY (or -INF) as end arguments (`x_1` or `x_n`). These are a flag that the function should be extended as a constant function.
* The value of such points must be equal to the nearest argument (x-value).
* `-v`:      Verbose mode (can be used up to 3 times for increased verbosity).
* `-i`:      After the described relation is turned into a function, the relation's coordinates are exchanged, and points are discarded from the resulting relation until it is again a function (this is in some sense an inverse function).
*`-x`:      If we encounter a relation with multiple equal arguments, keep the one with the largest value (y-value).

This tool can be used to infer parameters from the statistical results.

### Utilities to Produce Test Data
#### `randomfile`
Usage:
	`randomfile [-v] [-b <p>,<bits>] [-s <m>] [-d] [-a <andmask>] [-n <mean>,<stddev>]`
* `-d`: Make any RNG input deterministic.
* `-b <p>,<b>`: Produce b-bit symbols with Pr(0) = p, of each other symbol is (1-p)/(2^b - 1).
* `-c <c>`: Produce 1-bit symbols with correlation c, that is Pr(X_j = a| X_{j-1} = a) = (c+1)/2 (so -1 <= c <= 1).
* `-n <mean>,<stddev>,<bits>`: Produce truncated normally distributed samples, with the provided mean and standard deviation, fitting into `<bits>` bits.
* `-u <startBias>,<serialCoefficient>,<stepNoiseMean>,<stepNoiseStdDev>,<leftStepSize>,<rightStepSize>`: Produce bitwise data using the SUMS model.
* `-p <magnitude>,<period>`: Sinusoidal bias, with `<magnitude>` and `<period>` listed (each sample takes 1 time unit).
* `-s <m>`: Use a sample set of `<m>` values. (default m=1000000)
* `-f <b>,<p>`: Output `<b>` bits from an output filtered via a LFSR (`<p>` is the LFSR)
* `-a <andmask>`: AND the output with andmask
* `-l <len>`: length of the averaging block.
* `-v`: verbose mode.
* `-t`: Output uint8_t integers.
* outputs random uint32_t (uint8_t when "-t" option is used) integers to stdout

#### `simulate-osc`
Usage:
	`simulate-osc [-v] [-n <samples>] [-p <phase>] [-f] [-u] <ringOscFreq> <ringOscJitterSD> <sampleFreq>`
*`<sampleFreq>` Is either the sampling frequency or "*" followed by the number of nominal ring oscillations per sample cycle.
* `-v`: Enable verbose mode.
* `-s`: Send verbose mode output to stdout.
* `-n <samples>`: Number of samples to generate (default: 1000000)
* `-p <phase>`:  Specify the initial phase of the ring oscillator from the interval [0,1) (relative to the sample clock). Default: generate randomly.
* `-d`: Make any RNG input deterministic.
* `-o`: Output deviation from deterministic output in addition to the actual output.
* `-f`: Fudge the ringOscFreq using a normal distribution.
* `-u`:  Fudge the average intra-sample phase change. (Fix the number of cycles per sample, and select the ISP fractional part uniformly from [0, 0.25]).
* Frequency values are in Hz.
* Jitter Standard Deviation is in seconds (e.g. 1E-15) or a percentage of the ring oscillator period (ending with %, e.g., 0.001%).
* Output is uint8\_t integers; lsb is oscillator output. next bit (if enabled) tracks deviations from the deterministic value.

### Utilities to Help Interpret Results.
#### `percentile`
Usage:
	`percentile [-l] [-v] [-d] [-o] [-0] [-b <low>,<high>] <p> [filename]`
* Takes doubles from the file (if provided) or stdin (by default), 1 per line, and gives the pth percentile
* percentile estimated using the recommended NIST method (Hyndman and Fan's R6).
* See: http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm
* data is presumed to be passed in via stdin, consisting of a series of lines of doubles.
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-d`: Make any RNG deterministic.
* `-o`: Produce only one output. If there is a confidence interval, report the minimum value.
* `-l`: Treat the last value as an upper bound, rather than a data element. Report a single value, the min of the upper bound and the assessed value or smallest value in the confidence interval.
* `-b <c>,<rounds>`:  Produce `<c>`-BCa bootstrap confidence intervals using `<rounds>` of bootstrapping.
* `-u <low>,<high>`: Discard samples that are not in the range [low, high].
* `-0`: Read in doubles in machine-specific format.
* `<p>`: Return the pth percentile p in [0, 1]

#### `mean`
Usage:
	`mean [-v] [-d] [-o] [-0] [-b <c>,<rounds>] [-u <low>,<high>] [filename]`
* Takes doubles from the file (if provided) or stdin (by default), 1 per line, and gives the mean.
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-d`: Make any RNG deterministic.
* `-o`: Produce only one output. If there is a confidence interval, report the minimum value.
* `-b <c>,<rounds>`: Produce `<c>`-BCa bootstrap confidence intervals using `<rounds>` of bootstrapping.
* `-u <low>,<high>`: Discard samples that are not in the range [low, high].
* `-0`: Read in doubles in machine-specific format.

#### `failrate`
Usage:
	`failrate [-v] [-d] [-0] <p> <q> [filename]`
* Takes doubles from the file (if provided) or stdin (by default), 1 per line or in binary format, and gives the proportion that are less than or equal to p or greater than or equal to q.
* Useful to characterize false positive rates for statistical tests with inclusive failure bounds.
* data is presumed to be passed in via stdin, consisting of a series of lines of doubles.
* `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
* `-s`: Assume that the data is sorted.
* `-0`: Read in doubles in machine-specific format.
* `<p>`: Lower bound
* `<q>`: Upper bound

### Data Conversion Utilities
#### `u64-to-u32`
Usage:
	`u64-to-u32 [-r] [-t]`
* `-r`: Switch endianness of input values.
* `-t`: Truncate input values.
* The values are expected to be provided via stdin and the output via stdout.

#### `u64-counter-raw`
Usage:
	`u64-counter-raw <filename>`
* Extract deltas treated as 64-bit unsigned counters (that roll may roll over).

#### `u8-to-sd`
Usage:
	`u8-to-u32`
* The values are expected to be provided via stdin.

#### `u16-to-u32`
Usage:
	`u16-to-u32 [-d]`
* `-d`: output differences between adjacent values.
* The values are expected to be provided via stdin.

#### `u64-to-ascii`
Usage:
	`u64-to-ascii`
* The values are expected to be provided via stdin.

#### `u32-to-sd`
Usage:
	`u16-to-sd [-l] [-b]`
* Expand packed bits that are stored in u16 values.
* `-l`  extract bits from low bit to high bit
* `-b`  16 bit values are in big endian format.
* The values are expected to be provided via stdin.

#### `u16-to-sd`
Usage:
	`u16-to-sd [-l] [-b]`
* Expand packed bits that are stored in u16 values.
* `-l`: extract bits from low bit to high bit
* `-b`: 16 bit values are in big endian format.
* The values are expected to be provided via stdin.

#### `u64-jent-to-delta`
Usage:
	`u64-jent-to-delta`
* input comes from stdin are in uint64\_t, in the default jent delta format.
* output sent to stdout is the number of nanoseconds represented by the delta.

#### `u32-to-categorical`
Usage:
	`u32-to-categorical [-v] [-m] [-t <value>] <infile>`
* Produces categorical summary of data.
* `-v`: Increase verbosity. Can be used multiple times.
* `-m`: Output in Mathematica-friendly format.
* `-t <value>`: Trim any value that is prior to the first symbol with `<value>` occurrences or more or after the last symbol with `<value>` occurrences or more.
* `-z`: Don't output zero categories.
* The values are expected to be provided via stdin.

#### `dec-to-u32`
Usage:
	`dec-to-u32`
* The values are expected to be provided via stdin, one per line.

#### `u32-expand-bitwidth`
Usage:
	`u32-expand-bitwidth <filename>`
* Extract inferred values under the assumption that the data is a truncation of some sampled value, whose bitwidth is inferred.
* The values are expected to be provided via stdin.

#### `u32-to-ascii`
Usage:
	`u32-to-ascii`
* Prints the provided binary data as human-readable decimal values.
* The values are expected to be provided via stdin.
* The values are output by stdout.

#### `sd-to-hex`
Usage:
	`sd-to-hex`
* Output the bytes in hex format.
* The values are expected to be provided via stdin.
* The output values are sent to stdout.

#### `u8-to-u32`
Usage:
	`u8-to-u32`
* The values are expected to be provided via stdin.

#### `hex-to-u32`
Usage:
	`hex-to-u32`
* The values are expected to be provided via stdin, one per line.

#### `blocks-to-sd`
Usage:
	`blocks-to-sd [-l] <blocksize> <ordering>`
* Extract bits from a blocksize-sized block a byte at a time, in the specified byte ordering.
* blocksize        is the number of bytes per block
* ordering         is the indexing order for bytes (zero indexed decimal, separated by colons)
* -l  Extract bits from least to most significant within each byte.
* The values are expected to be provided via stdin.
* output is single bits stored in uint8\_t sent to stdout.
* example, standard little endian 32 bit integers, data stored least to most significant: `blocks-to-sd -l 4 0:1:2:3`

#### `u32-counter-endian`
Usage:
	`u32-counter-endian [-d] <infile>`
* Trys to guess counter endianness, and translates to the local platform.
* `-d`: output differences between adjacent values (when viewing the data as a 32-bit unsigned counter with rollover).

#### `u32-delta`
Usage:
	`u32-delta <filename>`
* Extract deltas and then translate the result to a positive value.
* The values are output via stdout.

#### `u32-counter-bitwidth`
Usage:
	`u32-counter-bitwidth <filename>`
* Extract deltas under the assumption that the data is an increasing counter of some inferred bitwidth.


#### `u32-counter-raw`
Usage:
	`u32-counter-raw <filename>`
* Extract deltas treated as 32-bit unsigned counters (that roll may roll over).

#### `u64-counter-endian`
Usage:
	`u64-counter-endian <filename>`
* Extract deltas treated as 64-bit unsigned counters (that roll may roll over).
* Attempt to determine the endianness.
* The values are expected to be provided via stdin.

## Make

Several `Makefile`s are provided; these are useful in various contexts which are hopefully reasonably clear from the name.

## More Information

For more information on the estimation methods, see [SP 800-90B](https://nvlpubs.nist.gov/nistpubs/SpecialPublications/NIST.SP.800-90B.pdf).
