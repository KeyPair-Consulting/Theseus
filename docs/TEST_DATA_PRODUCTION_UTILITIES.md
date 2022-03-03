# Theseus: Test Data Production Utilities

## randomfile
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

## simulate-osc
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
