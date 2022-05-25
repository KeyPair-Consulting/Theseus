# Theseus: Test Data Production Utilities

## randomfile
Usage:
	`randomfile [-v] [-b <p>,<b>] [-s <m>] [-d] [-a <andmask>] [-n <mean>,<stddev>,<bits>]`
* Create a random data file for use in testing.
* Input is not required.
* Output values of type uint32_t (or uint8_t when `-t` is used) are sent to stdout.
* Options:
    * `-v`: Verbose mode.
	* `-b <p>,<b>`: Produce `<b>`-bit symbols with Pr(0) = `<p>`, of each other symbol is (1-p)/(2^b - 1).
	* `-s <m>`: Use a sample set of `<m>` values (default m=1000000).
	* `-d`: Make any RNG input deterministic.
	* `-a <andmask>`: AND the output with `<andmask>`.
	* `-n <mean>,<stddev>,<bits>`: Produce truncated normally distributed samples, with the provided mean and standard deviation, fitting into `<bits>` bits.
	* `-c <c>`: Produce 1-bit symbols with correlation `<c>`, that is Pr(X_j = a| X_{j-1} = a) = (c+1)/2 (so -1 <= c <= 1).
	* `-u <startBias>,<serialCoefficient>,<stepNoiseMean>,<stepNoiseStdDev>,<leftStepSize>,<rightStepSize>`: Produce bitwise data using the SUMS model.
	* `-p <magnitude>,<period>`: Sinusoidal bias, with `<magnitude>` and `<period>` listed (each sample takes 1 time unit).
	* `-f <b>,<p>`: Output `<b>` bits from an output filtered via a LFSR (`<p>` is the LFSR).
	* `-l <len>`: Length of the averaging block.
	* `-t`: Output uint8_t values rather than uint32_t.
* Example TDPU01 - A random data file of 32 samples is generated and stdout is sent to a binary file with command `./randomfile -v -s 32 > tdpu01-output-u32.bin`: 
    * Output (viewed with command `xxd tdpu01-output-u32.bin`):
	  ```
	  00000000: f2b2 dc61 ee3d 31f2 51a7 e692 7645 6b42  ...a.=1.Q...vEkB
	  00000010: dfa2 75ff 8ffb 34a2 f219 1a21 4859 2385  ..u...4....!HY#.
	  00000020: 6376 bbe6 2002 80ab d1fa 9c13 7bae 96c8  cv.. .......{...
	  00000030: 6816 ad67 4168 6438 5585 c5c4 7007 df4b  h..gAhd8U...p..K
	  00000040: 572e e0d0 1239 9f51 ee0b af8c 1ac9 9dc9  W....9.Q........
	  00000050: e0f6 c905 334d 6cc0 2429 b945 0e4d 0a3b  ....3Ml.$).E.M.;
	  00000060: 5caf 83c3 835d 91e7 8f45 d9f1 cff1 45df  \....]...E....E.
	  00000070: da5a 8783 11fd 4468 f74f 067b fbf2 3a81  .Z....Dh.O.{..:.
	  ```
    * Output (to console):
	  ```
	  Outputting biased data
	  Output bits: 32, Bias: 2.3283064365386963e-10, Entropy: 32
	  Outputting 32 uint32_t values
	  ```

## simulate-osc
Usage:
	`simulate-osc [-v] [-n <samples>] [-p <phase>] [-f] [-u] <ringOscFreq> <ringOscJitterSD> <sampleFreq>`
* Create a random data file for use in testing that simulates a ring oscillator.
* Input values of type double (>=0) are provided in `<ringOscFreq>`, `<ringOscJitterSD>`, and `<sampleFreq>`.  Frequency values are in Hz. `<sampleFreq>` is either the sampling frequency or "*" followed by the number of nominal ring oscillations per sample cycle. `<ringOscJitterSD>` is in seconds (e.g., 1E-15) or a percentage of the ring oscillator period (ending with %, e.g., 0.001%).
* Output values of type uint8_t are sent to stdout. LSB is the oscillator output. The next bit (if enabled) tracks deviations from the deterministic value.
* Options:
    * `-v`: Verbose mode.
	* `-n <samples>`: Number of samples to generate (default: 1000000).
	* `-p <phase>`: Specify the initial phase of the ring oscillator from the interval [0,1) (relative to the sample clock). Default: generate randomly.
	* `-f`: Fudge the ringOscFreq using a normal distribution.
	* `-u`:  Fudge the average intra-sample phase change. (Fix the number of cycles per sample, and select the ISP fractional part uniformly from [0, 0.25]).
    * `-s`: Send verbose mode output to stdout.
    * `-d`: Make any RNG input deterministic.
    * `-o`: Output deviation from deterministic output in addition to the actual output.
* Example TDPU02 - A random data file of 32 samples is generated with parameters 1.0E9 0.1% 1.0E6 and stdout is sent to a binary file with command `./simulate-osc -v -n 32 1.0E9 0.1% 1.0E6 > tdpu02-output-u8.bin`: 
    * Output (viewed with command `xxd tdpu02-output-u8.bin`):
	  ```
      00000000: 0000 0000 0101 0100 0000 0000 0000 0000  ................
      00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
	  ```
    * Output (to console):
	  ```
      oscFreq: 1000000000
      oscJitter: 1.0000000000000002e-12
      sampleFreq: 1000000
      Initial Phase: 0.32743310348804799
      Complete RO cycles per sample: 1000, Nu: 0
      Normalized per-sample oscillator jitter: 0.031622776601683798
	  ```