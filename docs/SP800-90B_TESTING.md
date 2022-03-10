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
	`restart-transpose [-v] [-l <index>] [-d <samples>,<restarts>] <inputfile>`
* Create the transpose of the provided restart data matrix.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-v`: Verbose mode.
    * `-l <index>`: Read the `<index>` substring of length `<samples * restarts>` (default `index` = 0).
    * `-d <samples>,<restarts>`: Perform the restart testing using the described matrix dimensions (default `<samples>` X `<restarts>` is 1000 X 1000).
* Example 90B02 - A binary file is sent to stdin,`<samples>,<restarts>` is set to 16,4, and stdout is sent to a binary file with command `./restart-transpose -d 16,4 90b02-input-sd.bin > 90b02-output-0-sd.bin`: 
    * Input (viewed with command `xxd 90b02-input-sd.bin`):
	  ```
	  00000000: 0001 0203 0405 0607 0809 1011 1213 1415  ................
	  00000010: 1617 1819 2021 2223 2425 2627 2829 3031  .... !"#$%&'()01
	  00000020: 3233 3435 3637 3839 4041 4243 4445 4647  23456789@ABCDEFG
	  00000030: 4849 5051 5253 5455 5657 5859 6061 6263  HIPQRSTUVWXY`abc
	  ```
    * Output (if `<index>` = 0 (default), viewed with command `xxd 90b02-output-0-sd.bin`):
	  ```
	  00000000: 0004 0812 1620 2428 3236 4044 4852 5660  ..... $(26@DHRV`
	  00000010: 0105 0913 1721 2529 3337 4145 4953 5761  .....!%)37AEISWa
	  00000020: 0206 1014 1822 2630 3438 4246 5054 5862  ....."&048BFPTXb
	  00000030: 0307 1115 1923 2731 3539 4347 5155 5963  .....#'159CGQUYc
	  ```
    * Alternate Output (if `<index>` = 1 and `<samples>,<restarts>` = 16,2, viewed with command `xxd 90b02-output-1-sd.bin`):
	  ```
	  00000000: 3234 3638 4042 4446 4850 5254 5658 6062  2468@BDFHPRTVX`b
	  00000010: 3335 3739 4143 4547 4951 5355 5759 6163  3579ACEGIQSUWYac
	  ```

## restart-sanity
Usage:
  `restart-sanity [-t <n>] [-v] [-n] [-l <index> ] [-d <samples>,<restarts>] [-c <Xmaxcutoff>] [-i <rounds>] <H_I> <inputfile>` <br />
   or <br />
  `restart-sanity [-t <n>] [-v] [-n] [-k <m>] [-d <samples>,<restarts>] [-c <Xmaxcutoff>] [-i <rounds>] -r <H_I>`
* Perform the restart test for the provided restart data.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout.
* Options:
    * `<H_I>`: Required. The assessed initial entropy estimate as defined in SP 800-90B Section 3.1.3.
    * `-t <n>`: Uses `<n>` computing threads (default: number of cores * 1.3).
    * `-v`: Verbose mode (can be used several times for increased verbosity).
	* `-n`: Force counting a single fixed global symbol (causes the test to be binomial).
	* `-l <index>`: Read the `<index>` substring of length `<samples * restarts>` (default `index` = 0).
	* `-k <m>`: Use an alphabet of `<m>` values (default `m` = 2).
	* `-d <samples>,<restarts>`: Perform the restart testing using the described matrix dimensions (default `<samples>` X `<restarts>` is 1000 X 1000).
	* `-c <Xmaxcutoff>`: Use the provided cutoff (default `Xmaxcutoff` = 0).
	* `-i <rounds>`: Use `<rounds>` simulation rounds (default `rounds` = 2000000).
	* `-r`: Instead of doing testing on provided data use a random IID variable.
	* `-j <n>`: Each restart sanity vector is `<n>` elements long (default `n` = min(1000,`<samples>`,`<restart>`)).
	* `-m <t>,<simsym>`: For simulation, use `<t>` maximal size symbols (the residual probability is evenly distributed amongst the remaining `simsym-t` symbols) (default `<t>` = 0 and `<simsym>` = 0).
	* `-u`: Don't simulate the cutoff.
* Example 90B03 - A random data file is generated with -r, -vvv increases verbosity, -t 10 increases computing threads, -i 1000 decreases rounds (for testing), and `<H_I>` is set to 0.0123456 with command `./restart-sanity -r -vv -t 10 -i 1000 0.0123456`: 
    * Output (to console):
	  ```
	  Restart Sanity Test: p = 0.99147919179620048
	  Using 10 threads
	  Restart Sanity Test: Simulation Rounds = 1000
	  Restart Sanity Test: Simulation MLS Count = 1
	  Restart Sanity Test: Simulation Symbols = 2
	  Restart Sanity Test: Simulation MLS probability = 0.99147919179620048
	  Restart Sanity Test: Simulation other symbol probability = 0.0085208082037995236
	  Restart Sanity Test: Simulated XmaxCutoff = 998
	  At most 2 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Restart Sanity Test: X_R = 551 (row 759)
	  Restart Sanity Test: X_C = 551 (column 513)
	  Restart Sanity Test: X_max = 551
	  Restart Sanity Check Verdict: Pass
	  ```
    * Alternate Output (`<H_I>` is changed to 0.999988, to console):
	  ```
	  Restart Sanity Test: p = 0.50000415890037975
	  Using 10 threads
	  Restart Sanity Test: Simulation Rounds = 1000
	  Restart Sanity Test: Simulation MLS Count = 1
	  Restart Sanity Test: Simulation Symbols = 2
	  Restart Sanity Test: Simulation MLS probability = 0.50000415890037975
	  Restart Sanity Test: Simulation other symbol probability = 0.49999584109962025
	  Restart Sanity Test: Simulated XmaxCutoff = 549
	  At most 2 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Row over the cutoff: X_{C69} = 556
	  Row over the cutoff: X_{C175} = 558
	  Row over the cutoff: X_{C713} = 557
	  Restart Sanity Test: X_R = 558 (row 174)
	  Column over the cutoff: X_{C47} = 556
	  Column over the cutoff: X_{C125} = 551
	  Restart Sanity Test: X_C = 556 (column 46)
	  Restart Sanity Test: X_max = 558
	  Restart Sanity Check Verdict: Fail
	  ```

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