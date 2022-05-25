# Theseus: Executables that Implement Portions of SP 800-90B Testing

## non-iid-main
Usage:
	`non-iid-main [-v] [-s] [-b <bitmask>] [-l <index>,<samples>] <inputfile>` <br />
	or<br />
	`non-iid-main [-v] [-s] [-b <bitmask>] -R <k>,<L>`
* An implementation of the non-IID SP 800-90B estimators.  The final assessment is the minimum of the overall assessments.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 10 times for increased verbosity).
    * `-s`: Send verbose mode output to stdout.
	* `-b <bitmask>`: Only tests whose bits are set in `<bitmask>` are performed.
	* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
	* `-R <k>,<L>`: Randomly generate input data (`L` samples with `k` symbols) instead of reading a file.
    * `-i`: Calculate H\_bitstring and H\_I.
    * `-c`: Conditioned output, only calculate H\_bitstring.
    * `-r`: Raw evaluation, do not calculate H\_bitstring.
    * `-g`: Use little endian conventions for creation of the bitstring used in calculation of H\_bitstring (extract data least-significant to most-significant bits).
    * `-O <x>,<nu>`: Generate data from a simulated Ring Oscillator, with normalized jitter percentage of `<x>`% and normalized inter-sample expected phase change of `<nu>`. Negative `<nu>` forces random generation per-block.
	* `-f`: Generate a random `<nu>` value only once, rather than on a per-evaluation-block basis.
    * `-d`: Make all RNG operations deterministic.
    * `-L <x>`: Perform evaluations on blocks of size `<x>`. Prevents a single assessment of the entire file (but this can be turned back on using the `-S` option).
    * `-N <x>`: Perform `<x>` rounds of testing (only makes sense for randomly generated data).
    * `-B <c>,<rounds>`: Perform bootstrapping for a `<c>`-confidence interval using `<rounds>`.
    * `-P`: Establish an overall assessment based on a bootstrap of individual test parameters.
    * `-F`: Establish an overall assessment based on a bootstrap of final assessments.
    * `-S`: Establish an overall assessment using a large block assessment.
* Example 90B01 - A random data file of 1000000 samples is generated and tested with command `./non-iid-main -s -R 256,1000000`: 
    * Output (to console):
	  ```
	  Results for sole block
	  Literal Most Common Value Estimate: min entropy = 7.8862000559746326
	  Literal t-Tuple Estimate: min entropy = 7.3537577331212258
	  Literal LRS Estimate: min entropy = 7.9415957622713869
	  Literal MultiMCW Prediction Estimate: min entropy = 7.9480134217770955
	  Literal Lag Prediction Estimate: min entropy = 7.9347128011138919
	  Literal MultiMMC Prediction Estimate: min entropy = 7.9097172312204398
	  Literal LZ78Y Prediction Estimate: min entropy = 7.9096955922852317
	  H_original = 7.3537577331212258
	  Bitstring Most Common Value Estimate: min entropy = 0.99818739278509216
	  Bitstring Collision Estimate: min entropy = 0.93651002257606442
	  Bitstring Markov Estimate: min entropy = 0.9981971185464199
	  Bitstring Compression Estimate: min entropy = 0.883162374888803
	  Bitstring t-Tuple Estimate: min entropy = 0.92946758870669877
	  Bitstring LRS Estimate: min entropy = 0.99536007374538071
	  Bitstring MultiMCW Prediction Estimate: min entropy = 0.99879142512557872
	  Bitstring Lag Prediction Estimate: min entropy = 0.99897811408929016
	  Bitstring MultiMMC Prediction Estimate: min entropy = 0.99789060105788607
	  Bitstring LZ78Y Prediction Estimate: min entropy = 0.99743274366937418
	  H_bitstring = 0.883162374888803
	  Assessed min entropy = 7.065298999110424
	  ```
    * Output (if `<symbols>` is changed from 256 to 2 with -R 2,1000000 and verbosity 3 is added, to console):
	  ```
	  Verbosity set to 3
	  One bit symbols in use. Reverting to raw evaluation
	  Performing an assessment of size 1000000 symbols
	  1646937163 Generate 1000000 integers for round 1. Dataset preparation done.
	  Done with calculation
	  Results for sole block
	  Literal Most Common Value Estimate: Mode count = 500965
	  Literal Most Common Value Estimate: p-hat = 0.50096499999999999
	  Literal Most Common Value Estimate: p_u = 0.50225291289705221
	  Literal Most Common Value Estimate: min entropy = 0.99351406876115855
	  Test took 0.0019701240000000037 s CPU time
	  Literal Collision Estimate: v = 399945
	  Literal Collision Estimate: Sum t_i = 999999
	  Literal Collision Estimate: X-bar = 2.5003412969283278
	  Literal Collision Estimate: sigma-hat = 0.50000050860336853
	  Literal Collision Estimate: X-bar' = 2.4983047829760143
	  Literal Collision Estimate: p = 0.52911371690445652
	  Literal Collision Estimate: min entropy = 0.91835027579355322
	  Test took 0.0021115129999999954 s CPU time
	  Literal Markov Estimate: P_0 = 0.49903500000000001
	  Literal Markov Estimate: P_1 = 0.50096499999999999
	  Literal Markov Estimate: P_{0,0} = 0.49851513123354319
	  Literal Markov Estimate: P_{0,1} = 0.50148486876645681
	  Literal Markov Estimate: P_{1,0} = 0.49955186490074155
	  Literal Markov Estimate: P_{1,1} = 0.50044813509925845
	  Literal Markov Estimate: p-hat_max = 3.3512682267570183e-39
	  Literal Markov Estimate: min entropy = 0.99851944171796947
	  Test took 0.0088853640000000067 s CPU time
	  Literal Compression Estimate: X-bar = 5.2159441912543079
	  Literal Compression Estimate: sigma-hat = 1.0152913551751601
	  Literal Compression Estimate: X-bar' = 5.2095189258018459
	  Literal Compression Estimate: p = 0.031044430073819851
	  Literal Compression Estimate: min entropy = 0.8349202905688361
	  Test took 0.80123446000000009 s CPU time
	  Literal t-Tuple Estimate: t = 16
	  Literal t-Tuple Estimate: p-hat_max = 0.52755690708950331
	  Literal t-Tuple Estimate: p_u = 0.52884286485529719
	  Literal LRS Estimate: u = 17
	  Literal LRS Estimate: v = 38
	  Literal LRS Estimate: p-hat = 0.50190821942984742
	  Literal LRS Estimate: p_u = 0.50319612534616998
	  Literal t-Tuple Estimate: min entropy = 0.91908897702838832
	  Literal LRS Estimate: min entropy = 0.99080728150439557
	  Test took 0.12050002700000007 s CPU time
	  Literal MultiMCW Prediction Estimate: C = 499461
	  Literal MultiMCW Prediction Estimate: r = 20
	  Literal MultiMCW Prediction Estimate: N = 999937
	  Literal MultiMCW Prediction Estimate: P_global = 0.49949246802548558
	  Literal MultiMCW Prediction Estimate: P_global' = 0.50078042322898131
	  Literal MultiMCW Prediction Estimate: P_run = 0.62644742732673475
	  Literal MultiMCW Prediction Estimate: P_local can't change the result.
	  Literal MultiMCW Prediction Estimate: min entropy = 0.9977499301058671
	  Test took 0.065614129000000077 s CPU time
	  Literal Lag Prediction Estimate: C = 500328
	  Literal Lag Prediction Estimate: r = 19
	  Literal Lag Prediction Estimate: N = 999999
	  Literal Lag Prediction Estimate: P_global = 0.50032850032850029
	  Literal Lag Prediction Estimate: P_global' = 0.50161641599022688
	  Literal Lag Prediction Estimate: P_run = 0.38097708185627943
	  Literal Lag Prediction Estimate: P_local can't change the result.
	  Literal Lag Prediction Estimate: min entropy = 0.99534353207405335
	  Test took 0.11171224899999999 s CPU time
	  Literal MultiMMC Prediction Estimate: C = 500744
	  Literal MultiMMC Prediction Estimate: r = 20
	  Literal MultiMMC Prediction Estimate: N = 999998
	  Literal MultiMMC Prediction Estimate: P_global = 0.50074500149000301
	  Literal MultiMMC Prediction Estimate: P_global' = 0.50203291664399552
	  Literal MultiMMC Prediction Estimate: P_run = 0.61231217108182812
	  Literal MultiMMC Prediction Estimate: P_local can't change the result.
	  Literal MultiMMC Prediction Estimate: min entropy = 0.99414613485039294
	  Test took 0.13065326399999999 s CPU time
	  Literal LZ78Y Prediction Estimate: C = 500622
	  Literal LZ78Y Prediction Estimate: r = 20
	  Literal LZ78Y Prediction Estimate: N = 999983
	  Literal LZ78Y Prediction Estimate: P_global = 0.50063051071868225
	  Literal LZ78Y Prediction Estimate: P_global' = 0.50191843593783059
	  Literal LZ78Y Prediction Estimate: P_run = 0.61361995566326866
	  Literal LZ78Y Prediction Estimate: P_local can't change the result.
	  Literal LZ78Y Prediction Estimate: min entropy = 0.99447515626632654
	  Test took 0.12357909700000014 s CPU time
	  All tests took 1.3662768330000001 s CPU time
	  H_original = 0.8349202905688361
	  Assessed min entropy = 0.8349202905688361
	  ```

## restart-transpose
Usage:
	`restart-transpose [-v] [-l <index>] [-d <samples>,<restarts>] <inputfile>`
* Calculate the transpose of the provided restart data matrix.
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
	`permtests [-v] [-b <p>] [-t <n>] [-k <k>] [-d] [-s <m>] [-c] -r`
* Perform the permutation IID tests on the provided data.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used several times for increased verbosity).
	* `-t <n>`: uses `<n>` computing threads (default: number of cores * 1.3).
	* `-d`: Make any RNG input deterministic (also force one thread).
	* `-c`: Always complete all the tests.
	* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
	* `-b <p>`: Set the bias to `<p>`, that is the least likely symbol has probability `k^(-p)`. Default is 1 (i.e., unbiased.)
	* `-k <k>`: Use an alphabet of `<k>` values (default `k`=2).
	* `-s <m>`: Use a sample set of `<m>` values (default `m`=1000000).
	* `-r`: Instead of doing testing on provided data use a random IID variable.
* Example 90B04 - A random data file is generated with -r and tested with command `./permtests -r`: 
    * Output (to console):
	  ```
	  Getting data...
	  Excursion Test, GTR (C0): 6, EQR (C1): 0, LTR (C2): 127, Total: 133, Percentile: 0.95489, Verdict: Pass
	  Number of Directional Runs Test, GTR (C0): 46, EQR (C1): 0, LTR (C2): 52, Total: 98, Percentile: 0.53061, Verdict: Pass
	  Longest Directional Run Test, GTR (C0): 9, EQR (C1): 18, LTR (C2): 71, Total: 98, Percentile: 0.90816, Verdict: Pass
	  Numbers of Increases Directional Run Test, GTR (C0): 6, EQR (C1): 0, LTR (C2): 92, Total: 98, Percentile: 0.93878, Verdict: Pass
	  Number of Runs Test, GTR (C0): 6, EQR (C1): 1, LTR (C2): 39, Total: 46, Percentile: 0.86957, Verdict: Pass
	  Longest Runs Test, GTR (C0): 18, EQR (C1): 10, LTR (C2): 18, Total: 46, Percentile: 0.6087, Verdict: Pass
	  Mean Collision Test, GTR (C0): 6, EQR (C1): 0, LTR (C2): 40, Total: 46, Percentile: 0.86957, Verdict: Pass
	  Maximum Collision Test, GTR (C0): 40, EQR (C1): 1, LTR (C2): 5, Total: 46, Percentile: 0.13043, Verdict: Pass
	  Periodicity Test, p=1, GTR (C0): 10, EQR (C1): 0, LTR (C2): 80, Total: 90, Percentile: 0.88889, Verdict: Pass
	  Periodicity Test, p=2, GTR (C0): 24, EQR (C1): 0, LTR (C2): 66, Total: 90, Percentile: 0.73333, Verdict: Pass
	  Periodicity Test, p=8, GTR (C0): 21, EQR (C1): 0, LTR (C2): 69, Total: 90, Percentile: 0.76667, Verdict: Pass
	  Periodicity Test, p=16, GTR (C0): 42, EQR (C1): 0, LTR (C2): 48, Total: 90, Percentile: 0.53333, Verdict: Pass
	  Periodicity Test, p=32, GTR (C0): 16, EQR (C1): 0, LTR (C2): 74, Total: 90, Percentile: 0.82222, Verdict: Pass
	  Covariance Test, p=1, GTR (C0): 62, EQR (C1): 0, LTR (C2): 28, Total: 90, Percentile: 0.31111, Verdict: Pass
	  Covariance Test, p=2, GTR (C0): 6, EQR (C1): 0, LTR (C2): 84, Total: 90, Percentile: 0.93333, Verdict: Pass
	  Covariance Test, p=8, GTR (C0): 14, EQR (C1): 0, LTR (C2): 76, Total: 90, Percentile: 0.84444, Verdict: Pass
	  Covariance Test, p=16, GTR (C0): 50, EQR (C1): 0, LTR (C2): 40, Total: 90, Percentile: 0.44444, Verdict: Pass
	  Covariance Test, p=32, GTR (C0): 64, EQR (C1): 0, LTR (C2): 26, Total: 90, Percentile: 0.28889, Verdict: Pass
	  Compression results, GTR (C0): 6, EQR (C1): 0, LTR (C2): 14, Total: 20, Percentile: 0.7, Verdict: Pass
	  Testing could have concluded after 133 rounds
	  ```
    * Alternate Output (if `<k>` is set to 256 with -k 256 and verbosity 1 is added, to console):
	  ```
	  CPU Count: 2
	  Using 2 threads
	  Getting data...
	  At most 256 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Median of translated data: 128
	  Testing 1000000 samples with 256 symbols
	  1646939667 Assigned Round: 0 / 10000. No tests passed.
	  1646939672 Assigned Round: 18 / 10000. Finished tests: Compression
	  1646939672 Assigned Round: 22 / 10000. Finished tests: Runs Compression
	  1646939678 Assigned Round: 100 / 10000. Finished tests: Runs Compression
	  1646939682 Assigned Round: 152 / 10000. Finished tests: Excursion Runs Compression
	  1646939685 Assigned Round: 200 / 10000. Finished tests: Excursion Runs Compression
	  1646939692 Assigned Round: 300 / 10000. Finished tests: Excursion Runs Compression
	  1646939692 Assigned Round: 302 / 10000. Finished tests: Excursion Runs Collision Compression
	  1646939693 Assigned Round: 314 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939697 Assigned Round: 400 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939702 Assigned Round: 500 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939707 Assigned Round: 600 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939711 Assigned Round: 700 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939716 Assigned Round: 800 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939721 Assigned Round: 900 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939726 Assigned Round: 1000 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939730 Assigned Round: 1100 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939735 Assigned Round: 1200 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939740 Assigned Round: 1300 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939745 Assigned Round: 1400 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939750 Assigned Round: 1500 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939754 Assigned Round: 1600 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939759 Assigned Round: 1700 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939764 Assigned Round: 1800 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939769 Assigned Round: 1900 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939774 Assigned Round: 2000 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939778 Assigned Round: 2100 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939783 Assigned Round: 2200 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939788 Assigned Round: 2300 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939793 Assigned Round: 2400 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939797 Assigned Round: 2500 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  1646939802 Assigned Round: 2600 / 10000. Finished tests: Excursion Runs Collision Perodicity Compression
	  Data results:
	           Max excursion: 39142.910527098924
	           Number of directional runs: 665385
	           Longest directional run: 9
	           Maximum Changes: 501875
	           Number of runs: 499529
	           Longest run: 19
	           Mean Collision Dist: 20.623007280001652
	           Max Collision Dist: 75
	           Periodicity: 3927 3961 4019 3919 3820
	           Covariance: 16277374357 16265441845 16268807618 16261847758 16266365402
	           Compressed to: 1067580
	  Excursion Test, GTR (C0): 144, EQR (C1): 0, LTR (C2): 6, Total: 150, Percentile: 0.04, Verdict: Pass
	  Number of Directional Runs Test, GTR (C0): 2642, EQR (C1): 0, LTR (C2): 6, Total: 2648, Percentile: 0.0022659, Verdict: Pass
	  Longest Directional Run Test, GTR (C0): 131, EQR (C1): 915, LTR (C2): 1602, Total: 2648, Percentile: 0.95053, Verdict: Pass
	  Numbers of Increases Directional Run Test, GTR (C0): 1559, EQR (C1): 4, LTR (C2): 1085, Total: 2648, Percentile: 0.41125, Verdict: Pass
	  Number of Runs Test, GTR (C0): 13, EQR (C1): 0, LTR (C2): 7, Total: 20, Percentile: 0.35, Verdict: Pass
	  Longest Runs Test, GTR (C0): 14, EQR (C1): 4, LTR (C2): 2, Total: 20, Percentile: 0.3, Verdict: Pass
	  Mean Collision Test, GTR (C0): 294, EQR (C1): 0, LTR (C2): 6, Total: 300, Percentile: 0.02, Verdict: Pass
	  Maximum Collision Test, GTR (C0): 72, EQR (C1): 27, LTR (C2): 201, Total: 300, Percentile: 0.76, Verdict: Pass
	  Periodicity Test, p=1, GTR (C0): 120, EQR (C1): 1, LTR (C2): 191, Total: 312, Percentile: 0.61538, Verdict: Pass
	  Periodicity Test, p=2, GTR (C0): 71, EQR (C1): 2, LTR (C2): 239, Total: 312, Percentile: 0.77244, Verdict: Pass
	  Periodicity Test, p=8, GTR (C0): 6, EQR (C1): 0, LTR (C2): 306, Total: 312, Percentile: 0.98077, Verdict: Pass
	  Periodicity Test, p=16, GTR (C0): 139, EQR (C1): 3, LTR (C2): 170, Total: 312, Percentile: 0.55449, Verdict: Pass
	  Periodicity Test, p=32, GTR (C0): 290, EQR (C1): 0, LTR (C2): 22, Total: 312, Percentile: 0.070513, Verdict: Pass
	  Covariance Test, p=1, GTR (C0): 33, EQR (C1): 0, LTR (C2): 279, Total: 312, Percentile: 0.89423, Verdict: Pass
	  Covariance Test, p=2, GTR (C0): 264, EQR (C1): 0, LTR (C2): 48, Total: 312, Percentile: 0.15385, Verdict: Pass
	  Covariance Test, p=8, GTR (C0): 195, EQR (C1): 0, LTR (C2): 117, Total: 312, Percentile: 0.375, Verdict: Pass
	  Covariance Test, p=16, GTR (C0): 294, EQR (C1): 0, LTR (C2): 18, Total: 312, Percentile: 0.057692, Verdict: Pass
	  Covariance Test, p=32, GTR (C0): 239, EQR (C1): 0, LTR (C2): 73, Total: 312, Percentile: 0.23397, Verdict: Pass
	  Compression results, GTR (C0): 10, EQR (C1): 0, LTR (C2): 6, Total: 16, Percentile: 0.375, Verdict: Pass
	  Testing could have concluded after 2648 rounds
	  ```

## chisquare
Usage:
	`chisquare [-v] [-l <index>,<samples> ] <inputfile>` <br />
	or <br />
	`chisquare [-v] [-k <k>] [-s <m>] [-r]`
* Perform the chi square IID tests on the supplied data.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used several times for increased verbosity).
	* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
	* `-k <k>`:  Use an alphabet of `<k>` values (default `k`=2).
	* `-s <m>`:  Use a sample set of `<m>` values (default `m`=1000000).
	* `-r`:  Instead of doing testing on provided data use a random IID variable.
* Example 90B05 - A random data file is generated with -r and tested with command `./chisquare -r -v`: 
    * Output (to console):
	  ```
	  At most 2 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Testing 1000000 samples with 2 symbols
	  Binary Chi Squared Independence: m = 11
	  Bitmask is 0x000007FF
	  Binary Chi-Square Independence: Test Statistic = 2030.7019441237342
	  Binary Chi-Square Independence: df = 2046
	  Binary Chi-Square Independence: p-value = 0.59068953374298561
	  Binary Chi-Square Independence Test Verdict: Pass
	  Binary Chi-Square Goodness-of-Fit: Test Statistic = 14.174917619399288
	  Binary Chi-Square Goodness-of-Fit: p-value = 0.11623769841078238
	  Binary Chi-Square Goodness-of-Fit Test Verdict: Pass
	  ```
    * Alternate Output (if `<k>` is set to 256 with -k 256, to console):
	  ```
	  At most 256 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Testing 1000000 samples with 256 symbols
	  Non-Binary Chi-Square Independence: nbin = 65536
	  Non-Binary Chi-Square Independence: Test Statistic = 65141.610712809474
	  Non-Binary Chi-Square Independence: df = 65280
	  Non-Binary Chi-Square Independence: p-value = 0.64855582466782935
	  Non-binary Chi-Square Independence Test Verdict: Pass
	  Non-Binary Chi-Square Goodness-of-Fit: nbin = 256
	  Non-Binary Chi-Square Goodness-of-Fit: Test Statistic = 2254.9104436597504
	  Non-Binary Chi-Square Goodness-of-Fit: df = 2295
	  Non-Binary Chi-Square Goodness-of-Fit: p-value = 0.72084651029530566
	  Non-binary Chi-Square Goodness-of-Fit Test Verdict: Pass
	  ```

## lrs-test
Usage:
	`lrs-test [-v] [-l <index>,<samples> ] <inputfile>` <br />
	or <br />
	`lrs-test -r [-k <k>] [-s <m>]`
* Perform the LRS IID test on the supplied data.
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used several times for increased verbosity).
	* `-l <index>,<samples>`: Read the `<index>` substring of length `<samples>`.
    * `-r`: Instead of doing testing on provided data use a random IID variable.
    * `-k <k>`:  Use an alphabet of `<k>` values (default `k`=2).
    * `-s <m>`:  Use a sample set of `<m>` values (default `m`=1000000).
* Example 90B06 - A random data file is generated with -r and tested with command `./lrs-test -r -v`: 
    * Output (to console):
	  ```
	  At most 2 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Testing 1000000 samples with 2 symbols
	  p_col = 0.500000227137999999987
	  Longest repeated substring length: 40
	  Repeated string at positions 239292 and 929726
	  p_col^W = 9.095112283839325198847e-13
	  log(1-p_col^W)= -9.09511228384346125278e-13
	  N = 499960500780
	  Pr(X >= 1) = 0.3653741645115073903441
	  LRS Test Verdict: Pass
	  ```
    * Alternate Output (if `<k>` is set to 256 with -k 256, to console):
	  ```
	  At most 256 symbols.
	  Table based translation approach.
	  No translation is necessary.
	  Testing 1000000 samples with 256 symbols
	  p_col = 0.003907243775999999998615
	  Longest repeated substring length: 4
	  Repeated string at positions 842152 and 678702
	  p_col^W = 2.330676687477034238026e-10
	  log(1-p_col^W)= -2.330676687748636929255e-10
	  N = 499996500006
	  Pr(X >= 1) rounds to 1, but there is precision loss. The test verdict is still expected to be valid.
	  LRS Test Verdict: Pass
	  ```

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
* Example 90B07 - A binary file is given as input with command `./markov 90b07-input-sd.bin`: 
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