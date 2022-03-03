# Theseus: Executables that Implement Portions of SP 800-90B Testing

## non-iid-main
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
* Example 90B01 - Todo.

## restart-transpose
Usage:
	`restart-transpose [-v] [-l <index> ] [-d <samples>,<restarts>] <inputfile>`
* Calculate the transpose of the provided restart data matrix.
* `<inputfile>` is assumed to be a sequence of `statData_t` integers
* output is sent to stdout
* `-v`: verbose mode
* `-l <index>`: Read the `<index>` substring of length `<samples * restarts>`. (default index = 0)
* `-d <samples>,<restarts>`: Perform the restart testing using the described matrix dimensions. (default is 1000x1000)
* Example 90B02 - Todo.

## restart-sanity
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
* Example 90B03 - Todo.

## permtests
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
* Example 90B04 - Todo.

## chisquare
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
* Example 90B05 - Todo.

## lrs-test
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
* Example 90B06 - Todo.

## Markov
Usage:
	`markov [-v] [-p <cutoff>] [-c] <inputfile>`
* Run some variant of the SP 800-90B 2016 Markov test.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout. 
* Options:
    * `-v`: Verbose mode (can be used several times for increased verbosity).
    * `-p <cutoff>`: The lowest acceptable probability for a symbol to be relevant. Double value between 0.0 and 1.0.
    * `-c`: Disable the creation of confidence intervals.
* Example 90B07 - A binary file is given as input with command `./markov -vvv 90b07-input-sd.bin`: 
    * Input (viewed with command `xxd 90b07-input-sd.bin`):
	  ```
      00000000: 2122 2324 2526 2728 1011 1213 1415 1617  !"#$%&'(........
      00000010: 3132 3334 3536 3738 2122 2324 2526 2728  12345678!"#$%&'(
	  ```
    * Output (to console):
	  ```
      Read in 32 integers
      Running markov test.
      Assessed min entropy = 0.008342723774231342
	  ```
    * Alternate Output (if `-vvv` is used, to console):
	  ```	  
      Read in 32 integers
      Running markov test.
      At most 57 symbols.
      Table based translation approach.
      targetCount: 16, medianSlop: 0
      Prior count under target, newcount equal to target. Select space halfway to next symbol. Translation is necessary... Found 24 symbols total.
      Literal NSA Markov Estimate: Symbol cutoff probability is 0.
      Literal NSA Markov Estimate: Symbol cutoff count is 0.
      Literal NSA Markov Estimate: There are 24 sufficiently common distinct symbols in a dataset of 32 samples.
      Literal NSA Markov Estimate: alpha is 0.99998324958053397.
      Literal NSA Markov Estimate: Epsilon is 0.41452320609909138.
      Literal NSA Markov Estimate: Maximum Epsilon_i is 2.3448973599348508.
      Literal NSA Markov Estimate: Consider setting cutoff to at least 68.731795357867895.
      Literal NSA Markov Estimate: p = 0.47702320609909143
      Assessed min entropy = 0.008342723774231342
	  ```