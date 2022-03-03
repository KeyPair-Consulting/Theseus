# Theseus: Result Interpretation Utilities

## percentile
Usage:
	`percentile [-v] [-d] [-o] [-l] [-0] [-b <c>,<rounds>] [-u <low>,<high>] <p> [filename]`
* Takes a set of human-readable doubles and provides the pth percentile.  Percentile is estimated using the recommended NIST method [Hyndman and Fan's R6](http://www.itl.nist.gov/div898/handbook/prc/section2/prc262.htm).
* Input values in double format are provided via stdin (by default) or in `[filename]` (if provided), one per line.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
    * `-d`: Make any RNG deterministic.
    * `-o`: Produce only one output. If there is a confidence interval, report the minimum value.
    * `-l`: Treat the last value as an upper bound, rather than a data element. Report a single value, the min of the upper bound and the assessed value or smallest value in the confidence interval.
    * `-0`: Read in doubles in machine-specific binary format.
	* `-b <c>,<rounds>`:  Produce `<c>`-BCa bootstrap confidence intervals using `<rounds>` of bootstrapping.
    * `-u <low>,<high>`: Discard samples that are not in the range [low, high].
    * `<p>`: Required. Return the pth percentile where p is in the range [0, 1].
* Example RIU01 - A text file is given as input with a request for the 50% percentile with command `./percentile .50 riu01-input.txt`: 
    * Input (in riu01-input.txt, viewed with a text editor):
	  ```
	  0.111111
      2.074924
      2.145488
      2.196152
      2.029292
      2.784276
      3.000001
	  ```
	* Alternate Input (if `-0` is used and above data is now binary, viewed with command `xxd riu01-input-0-double.bin`):
	  ```
	  00000000: b3d1 393f c571 bc3f ea7b 0dc1 7199 0040  ..9?.q.?.{..q..@
      00000010: ffae cf9c f529 0140 255c c823 b891 0140  .....).@%\.#...@
      00000020: 8446 b071 fd3b 0040 344d d87e 3246 0640  .F.q.;.@4M.~2F.@
      00000030: 06bd 3786 0000 0840                      ..7....@
	  ```
    * Output (to console):
	  ```
      2.1454879999999998
	  ```
    * Alternate Output (if `-l` is used, to console):
	  ```	  
      2.1102059999999998
	  ```
    * Alternate Output (if `-b .95,10` is used, to console):
	  ```	  
      2.0749240000000002, 2.1454879999999998, 2.7842760000000002
	  ```
	* Alternate Output (if `-u 0.0,2.4999999999999999 ` is used, to console):
	  ```
	  2.0749240000000002
	  ```

## mean
Usage:
	`mean [-v] [-d] [-o] [-0] [-b <c>,<rounds>] [-u <low>,<high>] [filename]`
* Takes a set of human-readable doubles and provides the mean.
* Input values in double format are provided via stdin (by default) or in `[filename]` (if provided), one per line.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
    * `-d`: Make any RNG deterministic.
    * `-o`: Produce only one output. If there is a confidence interval, report the minimum value.
    * `-0`: Read in doubles in machine-specific binary format.
	* `-b <c>,<rounds>`: Produce `<c>`-BCa bootstrap confidence intervals using `<rounds>` of bootstrapping.
    * `-u <low>,<high>`: Discard samples that are not in the range [low, high].
* Example RIU02 - A text file is given as input with command `./mean riu02-input.txt`: 
    * Input (in riu02-input.txt, viewed with a text editor):
	  ```
	  0.111111
      2.074924
      2.145488
      2.196152
      2.029292
      2.784276
      3.000001
	  ```
    * Output (to console):
	  ```
	  2.0487491428571429
	  ```
    * Alternate Output (if `-b .95,10` is used, to console):
	  ```	  
      1.8055411428571428, 2.0487491428571429, 2.4306298571428573
	  ```
	  * Alternate Output (if `-u 2.0,3.0` is used, to console):
	  ```	  
      2.2460264000000003
	  ```

## failrate
Usage:
	`failrate [-v] [-s] [-0] <p> <q> [filename]`
* Takes a set of human-readable doubles and provides the proportion that are less than or equal to `<p>` or greater than or equal to `<q>`.  This is useful to characterize false positive rates for statistical tests with inclusive failure bounds.
* Input values in double format are provided via stdin (by default) or in `[filename]` (if provided), one per line.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
    * `-s`: Assume that the data is sorted.
    * `-0`: Read in doubles in machine-specific binary format.
    * `<p>`: Required. Lower bound as a double.
    * `<q>`: Required. Upper bound as a double.
* Example RIU03 - A text file is given as input, p is set to 0.1, and q is set to 3.0 with command `./failrate 0.1 3.0 riu03-input.txt`: 
    * Input (in riu03-input.txt, viewed with a text editor):
	  ```
	  0.111111
      2.074924
      2.145488
      2.196152
      2.029292
      2.784276
      3.000001
	  ```
    * Output (to console):
	  ```
	  Proportion in lower failure region: 0
      Proportion in upper failure region: 0.14285714285714285
      Proportion in failure region: 0.14285714285714285
	  ```