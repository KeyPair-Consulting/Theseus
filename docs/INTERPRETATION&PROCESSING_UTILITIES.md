# Theseus: Interpretation and Processing Utilities

## selectbits
Usage:
	`selectbits [-l <dir>] [-v] [-t <n>] [-c] <inputfile> <outputBits>`
* Identify the bit selections that are likely to contain the most entropy, up to `<outputBits>` bits wide.
* Input values of type uint32_t are provided in `<inputfile>`.
* Output of text summary is sent to stdout and additional logs are sent to `<dir>` (if `-l` used).
* Options:
    * `-l <dir>`:  Uses `<dir>` as the directory to store the log.  The file name stored will be `<inputfile>.select`.
    * `-v`: Verbose mode.
    * `-t <n>`: Uses `<n>` computing threads (default: number of cores * 1.3).  Positive integer less than or equal to 10,000.
    * `-c`: Conservative mode (use confidence intervals with the Markov estimator).
    * `<outputBits>`: Required. Used in selecting maximum bit width.  Positive integer that is less than or equal to the number of bits in `statData_t`. 
* Example IPU01 - A binary file is sent to stdin, `-l` is set to current directory, and `outputBits` is set to 3 with command `./selectbits -l . ipu01-input-u32.bin 3`: 
    * Input (viewed with command `xxd ipu01-input-u32.bin`):
	  ```
      00000000: 0000 0000 aaaa aaaa 1111 1111 ffff ffff  ................
      00000010: ffff ffff ffff ffff 0000 0000 1111 1111  ................
      00000020: aaaa aaaa 3333 3333 3333 3333 3333 3333  ....333333333333
      00000030: 3333 3333 0200 0000 0300 0000 0100 0000  3333............
	  ```
    * Output (in ipu01-input-u32.bin.select-3, viewed with a text editor):
	  ```
      Best entropy up to weight 1: 0.58687744211050441 (mask: 0x10000000)
      Best entropy up to weight 2: 0.68058696835394372 (mask: 0x00000003)
      Best entropy up to weight 3: 0.68058696835394372 (mask: 0x00000003)
      Final Best Masks: 
      0x10000000 (weight 1): 0.58687744211050441
      0x00000003 (weight 2): 0.68058696835394372
      test: 0x10000000 0x00000003 
      ```
    * Alternate Output (if `-c` is used, in ipu01-input-u32.bin.select-3c, viewed with a text editor):
	  ```	  
      Best entropy up to weight 1: 0 (mask: 0x00000001)
      Best entropy up to weight 2: 0.00088217340301532075 (mask: 0x10000002)
      Best entropy up to weight 3: 0.0028483489593084709 (mask: 0x50000002)
      Final Best Masks: 
      0x00000001 (weight 1): 0
      0x10000002 (weight 2): 0.00088217340301532075
      0x50000002 (weight 3): 0.0028483489593084709
      test: 0x00000001 0x10000002 0x50000002 
	  ```
    * Alternate Output (if `outputBits` = 8 and `-c` is used, in ipu01-input-u32.bin.select-8c, viewed with a text editor):
	  ```	  
      Best entropy up to weight 1: 0 (mask: 0x00000001)
      Best entropy up to weight 2: 0.00088217340301532075 (mask: 0x20000001)
      Best entropy up to weight 3: 0.0028483489593084709 (mask: 0x60000001)
      Best entropy up to weight 4: 0.0028483489593084709 (mask: 0x60000001)
      Final Best Masks: 
      0x00000001 (weight 1): 0
      0x20000001 (weight 2): 0.00088217340301532075
      0x60000001 (weight 3): 0.0028483489593084709
      test: 0x00000001 0x20000001 0x60000001 
	  ```

## highbin
Usage:
	`highbin <inputfile> <outputBits>`
* Attempts to bin input symbols into `2^<outputBits>` bins with equal numbers of adjacent samples.
* Input values of type uint32_t are provided in `<inputfile>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `<outputBits>`: Required. Used in calculating the number of bins.  Positive integer that is less than the number of bits in `statData_t`. 
* Example IPU02 - A binary file is sent to stdin, `outputBits` is set to 3, and stdout is sent to a binary file with command `./highbin ipu02-input-u32.bin 3 > ipu02-output-sd.bin`: 
    * Input (viewed with command `xxd ipu02-input-u32.bin`):
	  ```
      00000000: 0000 0000 aaaa aaaa 1111 1111 ffff ffff  ................
      00000010: ffff ffff ffff ffff 0000 0000 1111 1111  ................
      00000020: aaaa aaaa 3333 3333 3333 3333 3333 3333  ....333333333333
      00000030: 3333 3333 0200 0000 0300 0000 0100 0000  3333............
	  ```
    * Output (viewed with command `xxd ipu02-output-sd.bin`):
	  ```
      00000000: 0006 0407 0707 0004 0605 0505 0502 0301  ................
      ```
    * Additional Output (to console): 
	  ```
      Proceeding with 8 buckets
      Read in 16 samples
      Maximum symbol is 4294967295 (32 bit)
      Using a sorting approach
      Copying data...
      Sorting data...
      Tabulating symbols...
      Found 8 symbols
      Targeting 2 samples per bucket
      Bucketing by grouping symbols...
      BBGG: Inserting symbols into RBT.
      BBGG: Combining small adjacent interval pairs.
      BBGG: Extracting intervals.
      Score: 4 (8 bins created.)
      Testing resulting intervals...
      Bucketing by median splitting...
      BBMS: Targeting 8 buckets (3 initial cuts)
      BBMS: Initial cuts rendered 7 intervals
      Score: 4 (8 bins created.)
      Testing resulting intervals...
      Bucketing by greedy binning...
      BBOG: Found 2 over-numerous symbols (accounting for 7 samples). New target is 1
      Score: 4 (8 bins created.)
      Testing resulting intervals...
      Bucketing by greedy global grouping was more successful...
      Translating data...
	  ```

## translate-data
Usage:
	`translate-data [-v] [-n] <inputfile>`
* Perform an order-preserving map to arrange the input symbols to (0, ..., k-1).
* Input values of type statData_t (default uint8_t) are provided in `<inputfile>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
    * `-n`: No data output. Report number of symbols on stdout.
* Example IPU03 - A binary file is given as input and stdout is sent to a binary file with command `./translate-data ipu03-input-sd.bin > ipu03-output-sd.bin`: 
    * Input (viewed with command `xxd ipu03-input-sd.bin`):
	  ```
      00000000: 00aa ffff ffaa 0000 0000 0000 aaff 3333  ..............33
	  ```
    * Output (viewed with command `xxd ipu03-output-sd.bin`):
	  ```
      00000000: 0002 0303 0302 0000 0000 0000 0203 0101  ................
	  ```
    * Alternate Output (if `-v` is used, to console):
	  ```	  
      Read in 16 integers
      At most 256 symbols.
      Sorting based translation approach.
      Checking if we need to translate data: Translating data
      Found 4 symbols total.
      Found 4 symbols
	  ```

## u32-translate-data
Usage:
	`u32-translate-data [-v] [-n] <inputfile>`
* Perform an order-preserving map to arrange the input symbols to (0, ..., k-1).
* Input values of type uint32_t are provided in `<inputfile>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).
    * `-n`: No data output. Report number of symbols on stdout.
* Example IPU04 - A binary file is given as input and stdout is sent to a binary file with command `./u32-translate-data ipu04-input-u32.bin > ipu04-output-u32.bin`: 
    * Input (viewed with command `xxd ipu04-input-u32.bin`):
	  ```
      00000000: 0000 0000 aa00 0000 ff00 0000 ff00 0000  ................
      00000010: ff00 0000 aa00 0000 0000 0000 0000 0000  ................
      00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
      00000030: aa00 0000 ff00 0000 3300 0000 3300 0000  ........3...3...
	  ```
    * Output (viewed with command `xxd ipu04-output-u32.bin`):
	  ```
      00000000: 0000 0000 0200 0000 0300 0000 0300 0000  ................
      00000010: 0300 0000 0200 0000 0000 0000 0000 0000  ................
      00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
      00000030: 0200 0000 0300 0000 0100 0000 0100 0000  ................
	  ```
    * Alternate Output (if `-v` is used, to console):
	  ```	  
      Read in 16 integers
      At most 256 symbols.
      Sorting based translation approach.
      Checking if we need to translate data: Translating data
      Found 4 symbols total (max symbol 3).
      Found 4 symbols
	  ```