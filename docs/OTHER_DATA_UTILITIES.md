# Theseus: Other Data Utilities

## Functions to bin and group data:

### downsample
Usage:
	`downsample [-v] [-b <block size>] <rate> <filename>`
* Groups data by index into modular classes mod `<rate>` evenly into the `<block size>`.
* Input values of type statData_t (default uint8_t) are provided in `<filename>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-v`: Increase verbosity. 
    * `-b <block size>`: Samples per output block (default 1000000).  Read as an integer.
    * `<rate>`: Required.  Type uint32_t value identifying the number of input samples per output samples.  Read as an integer.
* Example ODU06 - A binary file is given as input, `-b <block size>` is set to 1, `rate` is set to 16, and stdout is sent to a binary file with command `./downsample -b 1 16 odu06-input-sd.bin > odu06-output-b1-16-sd.bin`: 
    * Input (viewed with command `xxd odu06-input-sd.bin`):
	  ```
      00000000: 0001 0203 0405 0607 0809 0a0b 0c0d 0e0f  ................
      00000010: 1011 1213 1415 1617 1819 1a1b 1c1d 1e1f  ................
      00000020: 2021 2223 2425 2627 2829 2a               !"#$%&'()*
	  ```
    * Output (viewed with command `xxd odu06-output-b1-16-sd.bin`):
	  ```
      00000000: 0010 0111 0212 0313 0414 0515 0616 0717  ................
      00000010: 0818 0919 0a1a 0b1b 0c1c 0d1d 0e1e 0f1f  ................
	  ```
	* Alternate Output (if `<block size>` = 1 and `<rate>` = 10, viewed with command `xxd odu06-output-b1-10-sd.bin`)
	  ```
      00000000: 000a 141e 010b 151f 020c 1620 030d 1721  ........... ...!
      00000010: 040e 1822 050f 1923 0610 1a24 0711 1b25  ..."...#...$...%
      00000020: 0812 1c26 0913 1d27                      ...&...'
	  ```
	* Alternate Output (if `<block size>` = 2 and `<rate>` = 8, viewed with command `xxd odu06-output-b2-8-sd.bin`)
	  ```
      00000000: 0008 1018 0109 1119 020a 121a 030b 131b  ................
      00000010: 040c 141c 050d 151d 060e 161e 070f 171f  ................
	  ```

### highbin
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

### u32-downsample
Usage:
	`u32-downsample [-v] [-b <block size>] <rate> <filename>`
* Groups data by index into modular classes mod `<rate>` evenly into the `<block size>`.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-v`: Increase verbosity. 
    * `-b <block size>`: Samples per output block (default 1000000).  Read as an integer.
    * `<rate>`: Required.  Type uint32_t value identifying the number of input samples per output samples.  Read as an integer.
* Example ODU23 - A binary file is given as input, `-b <block size>` is set to 1, `rate` is set to 16, and stdout is sent to a binary file with command `./u32-downsample -b 1 16 odu23-input-u32.bin > odu23-output-b1-16-u32.bin`: 
    * Input (viewed with command `xxd odu23-input-u32.bin`):
	  ```
	  00000000: 0000 0000 0100 0000 0200 0000 0300 0000  ................
	  00000010: 0400 0000 0500 0000 0600 0000 0700 0000  ................
	  00000020: 0800 0000 0900 0000 0a00 0000 0b00 0000  ................
	  00000030: 0c00 0000 0d00 0000 0e00 0000 0f00 0000  ................
	  00000040: 1000 0000 1100 0000 1200 0000 1300 0000  ................
	  00000050: 1400 0000 1500 0000 1600 0000 1700 0000  ................
	  00000060: 1800 0000 1900 0000 1a00 0000 1b00 0000  ................
	  00000070: 1c00 0000 1d00 0000 1e00 0000 1f00 0000  ................
	  00000080: 2000 0000 2100 0000 2200 0000 2300 0000   ...!..."...#...
	  00000090: 2400 0000 2500 0000 2600 0000 2700 0000  $...%...&...'...
	  000000a0: 2800 0000 2900 0000 2a00 0000            (...)...*...
	  ```
    * Output (viewed with command `xxd odu23-output-b1-16-u32.bin`):
	  ```
	  00000000: 0000 0000 1000 0000 0100 0000 1100 0000  ................
	  00000010: 0200 0000 1200 0000 0300 0000 1300 0000  ................
	  00000020: 0400 0000 1400 0000 0500 0000 1500 0000  ................
	  00000030: 0600 0000 1600 0000 0700 0000 1700 0000  ................
	  00000040: 0800 0000 1800 0000 0900 0000 1900 0000  ................
	  00000050: 0a00 0000 1a00 0000 0b00 0000 1b00 0000  ................
	  00000060: 0c00 0000 1c00 0000 0d00 0000 1d00 0000  ................
	  00000070: 0e00 0000 1e00 0000 0f00 0000 1f00 0000  ................
	  ```
	* Alternate Output (if `<block size>` = 1 and `<rate>` = 10, viewed with command `xxd odu23-output-b1-10-u32.bin`)
	  ```
	  00000000: 0000 0000 0a00 0000 1400 0000 1e00 0000  ................
	  00000010: 0100 0000 0b00 0000 1500 0000 1f00 0000  ................
	  00000020: 0200 0000 0c00 0000 1600 0000 2000 0000  ............ ...
	  00000030: 0300 0000 0d00 0000 1700 0000 2100 0000  ............!...
	  00000040: 0400 0000 0e00 0000 1800 0000 2200 0000  ............"...
	  00000050: 0500 0000 0f00 0000 1900 0000 2300 0000  ............#...
	  00000060: 0600 0000 1000 0000 1a00 0000 2400 0000  ............$...
	  00000070: 0700 0000 1100 0000 1b00 0000 2500 0000  ............%...
	  00000080: 0800 0000 1200 0000 1c00 0000 2600 0000  ............&...
	  00000090: 0900 0000 1300 0000 1d00 0000 2700 0000  ............'...
	  ```
	* Alternate Output (if `<block size>` = 2 and `<rate>` = 8, viewed with command `xxd odu23-output-b2-8-u32.bin`)
	  ```
	  00000000: 0000 0000 0800 0000 1000 0000 1800 0000  ................
	  00000010: 0100 0000 0900 0000 1100 0000 1900 0000  ................
	  00000020: 0200 0000 0a00 0000 1200 0000 1a00 0000  ................
	  00000030: 0300 0000 0b00 0000 1300 0000 1b00 0000  ................
	  00000040: 0400 0000 0c00 0000 1400 0000 1c00 0000  ................
	  00000050: 0500 0000 0d00 0000 1500 0000 1d00 0000  ................
	  00000060: 0600 0000 0e00 0000 1600 0000 1e00 0000  ................
	  00000070: 0700 0000 0f00 0000 1700 0000 1f00 0000  ................
	  ```

### u32-manbin
Usage:
	`u32-manbin <filename> <cutoff_1 ... cutoff_{n-1}>`
* Assign given binary data to one of the n bin numbers (0, ..., n-1).  
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint8_t are sent to stdout.
* Options:
    * `<cutoff_1 ... cutoff_{n-1}>`: Required. Set of integer values separated by spaces where the total number of bins must <= 256.  The cutoffs specify the first value in the next bin (so the first bin is `[0, cutoff_1)`, the second bin is `[cutoff_1, cutoff_2)`, the last bin is `[cutoff_{n-1}, UINT32_MAX ]`, etc.).
* Example ODU16 - A binary file is sent to stdin, `cutoffs` are set to 5 10 15 20, and stdout is sent to a binary file with command `./u32-manbin odu16-input-u32.bin 5 10 15 20 > odu16-output-u8.bin`: 
    * Input (viewed with command `xxd odu16-input-u32.bin`):
	  ```
      00000000: 0000 0000 0100 0000 0200 0000 0300 0000  ................
      00000010: 0400 0000 0500 0000 0600 0000 0700 0000  ................
      00000020: 0800 0000 0900 0000 0a00 0000 0b00 0000  ................
      00000030: 0f00 0000 1000 0000 1400 0000 ffff ffff  ................
	  ```
    * Output (viewed with command `xxd odu16-output-u8.bin`):
	  ```
      00000000: 0000 0000 0001 0101 0101 0202 0303 0404  ................
      ```
    * Additional Output (to console): 
	  ```
      A total of 5 output bins.
      [ 0, 5 ), [ 5, 10 ), [ 10, 15 ), [ 15, 20 ), [ 20, 4294967295 ]
      Read in 16 samples
      Outputting the data...
	  ```

## Functions to select and extract data:

### extractbits
Usage:
	`extractbits <filename> <bitmask>`
* Takes the given binary data and extracts bits with `<bitmask>`.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `<bitmask>`: Required. Type uint32_t value identifying the bitmask.  Read as an integer.
* Example ODU07 - A binary file is given as input, `bitmask` is set to 10 (0x0A000000) , and stdout is sent to a binary file with command `./extractbits odu07-input-u32.bin 10 > odu07-output-sd.bin`: 
    * Input (viewed with command `xxd odu07-input-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000                      ........
	  ```
    * Output (viewed with command `xxd odu07-output-sd.bin`):
	  ```
      00000000: 0001 0100 0001 0102 0203                 ..........
	  ```
    * Additional Output (to console):
	  ``` 
      Input bitmask 0xA (Hamming weight: 2)
      Read in 10 uint32_ts
      Outputting data
	  ```

### selectbits
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

### u32-bit-select
Usage:
	`u32-bit-select [-r] <bit>`
* Selects and returns the value in the given bit position (0 is the LSB, 31 is the MSB).
* Input values of type uint32_t are provided via stdin.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-r`: Reverse the endianness of input values before selecting the specified bit.
    * `<bit>`: Required. Integer value between 0 and 31 representing the bit position of interest.
* Example ODU13 - A binary file is sent to stdin, `<bit>` is set to 0, and stdout is sent to a binary file with command `./u32-bit-select 0 < odu13-input-u32.bin > odu13-output-0-sd.bin`: 
    * Input (viewed with command `xxd odu13-input-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000                      ........
	  ```
    * Output (viewed with command `xxd odu13-output-0-sd.bin`):
	  ```
      00000000: 0100 0100 0100 0100 0100                 ..........
      ```
	* Alternate Output (if `<bit>` = 1, viewed with command `xxd odu13-output-1-sd.bin`)
	  ```
      00000000: 0001 0100 0001 0100 0001                 ..........
	  ```
	* Alternate Output (if `<bit>` = 2, viewed with command `xxd odu13-output-2-sd.bin`)
	  ```
      00000000: 0000 0001 0101 0100 0000                 ..........
	  ```

### u128-bit-select
Usage:
	`u128-bit-select <bit>`
* Selects and returns the value in the given bit position (0 is the LSB, 127 is the MSB, little endian is assumed).
* Input values of type uint128_t (read as 2 uint64_t values) are provided via stdin.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `<bit>`: Required. Integer value between 0 and 127 representing the bit position of interest.
* Example ODU09 - A binary file is sent to stdin, `<bit>` is set to 0, and stdout is sent to a binary file with command `./u128-bit-select 0 < odu09-input-u128.bin > odu09-output-0-sd.bin`: 
    * Input (viewed with command `xxd odu09-input-u128.bin`):
	  ```
      00000000: 0100 0000 0000 0000 0000 0000 0000 0000  ................
      00000010: 0200 0000 0000 0000 0000 0000 0000 0000  ................
      00000020: 0300 0000 0000 0000 0000 0000 0000 0000  ................
      00000030: 0400 0000 0000 0000 0000 0000 0000 0000  ................
      00000040: 0500 0000 0000 0000 0000 0000 0000 0000  ................
      00000050: 0600 0000 0000 0000 0000 0000 0000 0000  ................
      00000060: 0700 0000 0000 0000 0000 0000 0000 0000  ................
      00000070: 0800 0000 0000 0000 0000 0000 0000 0000  ................
      00000080: 0900 0000 0000 0000 0000 0000 0000 0000  ................
      00000090: 0a00 0000 0000 0000 0000 0000 0000 0000  ................
	  ```
    * Output (viewed with command `xxd odu09-output-0-sd.bin`):
	  ```
      00000000: 0100 0100 0100 0100 0100                 ..........
      ```
	* Alternate Output (if `<bit>` = 1, viewed with command `xxd odu09-output-1-sd.bin`)
	  ```
      00000000: 0001 0100 0001 0100 0001                 ..........
	  ```
	* Alternate Output (if `<bit>` = 2, viewed with command `xxd odu09-output-2-sd.bin`)
	  ```
      00000000: 0000 0001 0101 0100 0000                 ..........
	  ```

### u32-selectdata
Usage:
	`u32-selectdata <filename> <trimLowPercent> <trimHighPercent>`
* Attempt to keep the percentages noted in the provided binary data.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `<trimLowPercent>`: Required. Double value between 0 and 1.
    * `<trimHighPercent>`: Required. Double value between 0 and 1.
* Example ODU18 - A binary file is sent to stdin, trimLowPercent is set to 30%, tripHighPercent is set to 10%, and stdout is sent to a binary file with command `./u32-selectdata odu18-input-u32.bin .3 .1 > odu18-output-u32.bin`: 
    * Input (viewed with command `xxd odu18-input-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000                      ........
	  ```
    * Output (viewed with command `xxd odu18-output-u32.bin`):
	  ```
      00000000: 0200 0000 0300 0000 0400 0000 0500 0000  ................
      00000010: 0600 0000 0700 0000 0800 0000            ............
	  ```
    * Additional Output (to console):
	  ``` 
      Read in 10 samples
      Copying data
      Sorting data
      Getting the 1 to the 7 entries
      MinValue = 2
      MaxValue = 8
      Outputting the data...
	  ```

### u32-selectrange
Usage:
	`u32-selectrange <filename> <low> <high>`
* Extracts all values from the given binary data between a specified `low` and `high` (inclusive).
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `<low>`: Required. Type uint32_t value identifying the lowest value in the range to select.
    * `<high>`: Required. Type uint32_t value identifying the highest value in the range to select.
* Example ODU19 - A binary file is sent to stdin, low is set to 16, high is set to 255, and stdout is sent to a binary file with command `./u32-selectrange odu19-input-u32.bin 16 255 > odu19-output-u32.bin`: 
    * Input (viewed with command `xxd odu19-input-u32.bin`):
	  ```
      00000000: 0100 0000 1000 0000 1100 0000 ff00 0000  ................
      00000010: 0001 0000 0011 0000 ff00 0000 0f00 0000  ................
	  ```
    * Output (viewed with command `xxd odu19-output-u32.bin`):
	  ```
      00000000: 1000 0000 1100 0000 ff00 0000 ff00 0000  ................
	  ```
    * Additional Output (to console):
	  ``` 
      Read in 8 samples
      Outputting data in the interval [16, 255]
      Outputting the data...
	  ```

## Functions to translate data:

### translate-data
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

### u32-translate-data
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

## Functions to find and process fixed data:

### bits-in-use
Usage:
	`bits-in-use <filename>`
* Determines the number of bits required to represent the given data after removing stuck and superfluous bits.
* Input values of type uint32_t are provided in `<filename>`.
* Output of text summary is sent to stdout.
* Example ODU01 - A binary file is given as input with command `./bits-in-use odu01-input-u32.bin`: 
    * Input (viewed with command `xxd odu01-input-u32.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768                      abcdefgh
	  ```
    * Output (to console):
	  ```
      Read in 6 uint32_ts
      Non-fixed bits: 0x7F757775.
      Discarding bits equivalent to bit 30: 0x13515351.
      Discarding bits equivalent to bit 29: 0x00202020.
      Discarding bits equivalent to bit 18: 0x00000404.
      Bits to analyze: 0x6C040000.
      Found bitmask 0x6c040000
      5
	  ```

### discard-fixed-bits
Usage:
	`discard-fixed-bits <filename>`
* Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.
* Input values of type statData_t (default uint8_t) are provided in `<filename>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Example ODU02 - A binary file is given as input and stdout is sent to a binary file with command `./discard-fixed-bits odu02-input-sd.bin > odu02-output-sd.bin`: 
    * Input (viewed with command `xxd odu02-input-sd.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768                      abcdefgh
      ```
    * Output (viewed with command `xxd odu02-output-sd.bin`): 
	  ```
      00000000: 2122 2324 2526 2728 1011 1213 1415 1617  !"#$%&'(........
      00000010: 3132 3334 3536 3738                      12345678
	  ```
    * Additional Output (to console): 
	  ```
      Read in 24 uint8_ts
      Non-fixed bits: 0x0000007F.
      Discarding bits equivalent to bit 6: 0x00000010.
      Bits to analyze: 0x0000006F.
      Symbols in the range [48, 104], 7 bit, bitmask: 0x0000006F
      Outputting data
	  ```

### u32-discard-fixed-bits
Usage:
	`u32-discard-fixed-bits <filename>`
* Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Example ODU14 - A binary file is given as input and stdout is sent to a binary file with command `./u32-discard-fixed-bits odu14-input-u32.bin > odu14-output-u32.bin`: 
    * Input (viewed with command `xxd odu14-input-u32.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768                      abcdefgh
      ```
    * Output (viewed with command `xxd odu14-output-u32.bin`): 
	  ```
      00000000: 1200 0000 1500 0000 0800 0000 0b00 0000  ................
      00000010: 1a00 0000 1d00 0000                      ........
	  ```
    * Additional Output (to console): 
	  ```
      Read in 6 uint32_ts
      Symbols in the range [858927408, 1751606885], 31 bit, bitmask: 0x6C040000
      Outputting data
	  ```

### u128-discard-fixed-bits
Usage:
	`u128-discard-fixed-bits <filename> <outputgroup>`
* Takes provided binary data and returns it with fixed bits discarded. Non-fixed bits are moved to the LSB of the output.
* Input values of type uint128_t (read as 4 uint32_t values) are provided in `<filename>`.
* Output values of type uint32_t located at `<outputgroup>` (an integer that specifies location in the file) are sent to stdout.
* Example ODU10 - A binary file is given as input, `<outputgroup>` is specified as 3, and stdout is sent to a binary file with command `./u128-discard-fixed-bits odu10-input-u128.bin 3 > odu10-output-3-u32.bin`: 
    * Input (viewed with command `xxd odu10-input-u128.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768 4142 4344 4546 4748  abcdefghABCDEFGH
      00000020: 3031 3233 3435 3637 6162 6364 6566 6768  01234567abcdefgh
      ```
    * Output (viewed with command `xxd odu10-output-3-u32.bin`): 
	  ```
      00000000: 0100 0000 0200 0000 0300 0000            ............
	  ```
    * Alternate Output (if `<outputgroup>` = 1, viewed with command `xxd odu10-output-1-u32.bin`):
	  ```
      00000000: 0200 0000 0300 0000 0100 0000            ............
	  ```
    * Additional Output (to console): 
	  ```
      Read in 12 uint32_ts
      Symbols in the range [858927408, 1684234849] (31 bit: bitmask 0x60000000)
      Symbols in the range [926299444, 1751606885] (31 bit: bitmask 0x60000000)
      Symbols in the range [858927408, 1684234849] (31 bit: bitmask 0x60000000)
      Symbols in the range [926299444, 1751606885] (31 bit: bitmask 0x60000000)
      8 bits total
      Outputting group 3
	  ```

## Functions to merge, sort, and permute data:

### double-merge
Usage:
	`double-merge <file1> <file2> <outfile>`
* Merges two sorted lists of doubles into a single merged sorted list of doubles.
* Input values in binary double format are provided in `<file1>` and `<file2>`.
* Output values in binary double format are sent to `<outfile>`.
* Example ODU03 - Two pre-sorted binary files are given as input and a new file is identified for output with command `./double-merge odu03-input1-double.bin odu03-input2-double.bin odu03-output-double.bin`: 
    * Input 1 (viewed with command `xxd odu03-input1-double.bin`):
	  ```
      00000000: 1b9d f353 1cc7 f13f a96b ed7d aaaa 0a40  ...S...?.k.}...@
      00000010: 6284 f068 e338 1640 f052 ea92 711c 1f40  b..h.8.@.R..q..@
	  ```
	* Input 2 (viewed with command `xxd odu03-input2-double.bin`):
	  ```
      00000000: 1b9d f353 1cc7 0140 1b9d f353 1cc7 1140  ...S...@...S...@
      00000010: a96b ed7d aaaa 1a40 1b9d f353 1cc7 2140  .k.}...@...S..!@
	  ```
    * Output (viewed with command `xxd odu03-output-double.bin`):
	  ```
	  00000000: 1b9d f353 1cc7 f13f 1b9d f353 1cc7 0140  ...S...?...S...@
      00000010: a96b ed7d aaaa 0a40 1b9d f353 1cc7 1140  .k.}...@...S...@
      00000020: 6284 f068 e338 1640 a96b ed7d aaaa 1a40  b..h.8.@.k.}...@
      00000030: f052 ea92 711c 1f40 1b9d f353 1cc7 2140  .R..q..@...S..!@
	  ```

### double-sort
Usage:
	`double-sort <filename>`
* Takes doubles from the file and sorts them.
* Input values in binary double format are provided in `<filename>`.
* Output values in binary double format are sent to stdout.
* Example ODU05 - A binary file is given as input and stdout is sent to a binary file with command `./double-sort odu05-input-double.bin > odu05-output-double.bin`: 
    * Input (viewed with command `xxd odu05-input-double.bin`):
	  ```
	  00000000: 6284 f068 e338 1640 f052 ea92 711c 1f40  b..h.8.@.R..q..@
      00000010: a96b ed7d aaaa 0a40 1b9d f353 1cc7 f13f  .k.}...@...S...?
	  ```
    * Output (viewed with command `xxd odu05-output-double.bin`):
	  ```
	  00000000: 1b9d f353 1cc7 f13f a96b ed7d aaaa 0a40  ...S...?.k.}...@
      00000010: 6284 f068 e338 1640 f052 ea92 711c 1f40  b..h.8.@.R..q..@
	  ```

### u32-bit-permute
Usage:
	`u32-bit-permute [-r] <bit specification>`
* Permute bits within the given binary data as specified in `<bit specification>`.  Bit ordering is specified in the LSB 0 format (i.e., bit 0 is the LSB and bit 31 is the MSB).
* Input values of type uint32_t are provided via stdin.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-r`: Reverse the endianness of input values before permuting.
    * `<bit specification>`: Required. Ordered integer values between 0 and 31 representing the bit specification of interest.  Ordering of the bit specification is left to right, MSB to LSB, so the specification "31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0" is the identity permutation.  If fewer than 32 output bits are within the specification, the unspecified high order bits are set to 0.  Each bit position can be present at most once.
* Example ODU12 - A binary file is sent to stdin, `<bit specification>` is set to 31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0 (the identity permutation), and stdout is sent to a binary file with command `./u32-bit-permute 31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16:15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0 < odu12-input-u32.bin > odu12-output-identity-u32.bin`: 
    * Input (viewed with command `xxd odu12-input-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000                      ........
	  ```
    * Output (viewed with command `xxd odu12-output-identity-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000                      ........
      ```
	* Alternate Output (if `-r` is used, viewed with command `xxd odu12-output-identity-r-u32.bin`)
	  ```
      00000000: 0000 0001 0000 0002 0000 0003 0000 0004  ................
      00000010: 0000 0005 0000 0006 0000 0007 0000 0008  ................
      00000020: 0000 0009 0000 000a                      ........
	  ```
	* Alternate Output (if `<bit specification>` is set to 15:14:13:12:11:10:9:8:7:6:5:4:3:2:1:0:31:30:29:28:27:26:25:24:23:22:21:20:19:18:17:16, viewed with command `xxd odu12-output-swap-u32.bin`)
	  ```
      00000000: 0000 0100 0000 0200 0000 0300 0000 0400  ................
      00000010: 0000 0500 0000 0600 0000 0700 0000 0800  ................
      00000020: 0000 0900 0000 0a00                      ........

	  ```
	* Alternate Output (if `<bit specification>` is set to 15:13:11:9:7:5:3:1:31:29:27:25:23:21:19:17, viewed with command `xxd odu12-output-swapRemoveEven-u32.bin`)
	  ```
      00000000: 0000 0000 0001 0000 0001 0000 0000 0000  ................
      00000010: 0000 0000 0001 0000 0001 0000 0002 0000  ................
      00000020: 0002 0000 0003 0000                      ........
	  ```

## Functions with mathematical operations on data:

### double-minmaxdelta
Usage:
	`double-minmaxdelta [-v] [-0] [filename]`
* Takes a set of human-readable doubles and provides the mean.
* Input values in double format are provided via stdin (by default) or in `[filename]` (if provided), one per line.
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Verbose mode (can be used up to 3 times for increased verbosity).  This includes min and max values.
    * `-0`: Read in doubles in machine-specific binary format.
* Example ODU04 - A text file is given as input with command `./double-minmaxdelta odu04-input.txt`: 
    * Input (in odu04-input.txt, viewed with a text editor):
	  ```
	  0.111111
      2.074924
      2.145488
      2.196152
      2.029292
      2.784276
      3.000001
	  ```
	* Alternate Input (if `-0` is used and above data is now binary, viewed with command `xxd odu04-input-0-double.bin`):
	  ```
	  00000000: b3d1 393f c571 bc3f ea7b 0dc1 7199 0040  ..9?.q.?.{..q..@
      00000010: ffae cf9c f529 0140 255c c823 b891 0140  .....).@%\.#...@
      00000020: 8446 b071 fd3b 0040 344d d87e 3246 0640  .F.q.;.@4M.~2F.@
      00000030: 06bd 3786 0000 0840                      ..7....@
	  ```
    * Output (to console):
	  ```
	  2.88889
	  ```
    * Alternate Output (if `-v` is used, to console):
	  ```	  
      Max: 3.0000010000000001
      Min: 0.111111
      2.88889
	  ```

### hweight
Usage:
	`hweight <bitmask>`
* Calculates the Hamming weight of `<bitmask>`.  As an example, the bit string 11101000 has a Hamming weight of 4.
* Input value `<bitmask>` of type uint32_t is a required argument that is read as an integer.
* Output of text summary is sent to stdout.
* Example ODU08 - `<bitmask>` is set to 16711935 (0xFF00FF00) with command `./hweight 16711935`: 
    * Output (to console):
	  ```
      16
	  ```

### u16-mcv
Usage:
	`u16-mcv`
* Finds the most common value in the given binary data.
* Input values of type uint16_t are provided via stdin.
* Output of text summary is sent to stdout.
* Example ODU21 - A binary file is given as input with command `./u16-mcv < odu21-input-u16.bin`: 
    * Input (viewed with command `xxd odu21-input-u16.bin`):
	  ```
	  00000000: 0000 0000 aaaa aaaa 1111 1111 ffff ffff  ................
	  00000010: ffff ffff ffff ffff 0000 0000 1111 1111  ................
	  00000020: aaaa aaaa 3333 3333 3333 3333 3333 3333  ....333333333333
	  00000030: 3333 3333 0200 0000 0300 0000 0100 0000  3333............
	  ```
    * Output (to console):
	  ``` 
	  Most common symbol is 0x3333 (count 8)
	  p_hat = 0.25
	  p_u = 0.45032579212384927
	  minentropy = 1.150958985398685
	  ```

### u32-anddata
Usage:
	`u32-anddata [filename] <bitmask>`
* Takes the given binary data and bitwise ANDs each symbol with `<bitmask>`.
* Input values of type uint32_t are provided via stdin (by default) or in `[filename]` (if provided).
* Output values of type uint32_t are sent to stdout.
* Options:
    * `<bitmask>`: Required. Type uint32_t value identifying the bitmask for performing AND operations.  Read as an integer.
* Example ODU11 - A binary file is given as input, `bitmask` is set to 16711935, and stdout is sent to a binary file with command `./u32-anddata odu11-input-u32.bin 16711935 > odu11-output-u32.bin`: 
    * Input (viewed with command `xxd odu11-input-u32.bin`):
	  ```
      00000000: 8040 2010 0804 0201 f00f ff00 ff00 ff00  .@ .............
	  ```
    * Output (viewed with command `xxd odu11-output-u32.bin`):
	  ```
      00000000: 8000 2000 0800 0200 f000 ff00 ff00 ff00  .. .............
	  ```
    * Additional Output (to console):
	  ``` 
      Andmask: 0x00ff00ff
      Outputting data
	  ```

### u32-gcd
Usage:
	`u32-gcd <filename>`
* Finds common divisors and removes these factors from the given binary data.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Example ODU15 - A binary file is given as input and stdout is sent to a binary file with command `./u32-gcd odu15-input-u32.bin > odu15-output-u32.bin`: 
    * Input (viewed with command `xxd odu15-input-u32.bin`):
	  ```
      00000000: 8040 2010 0804 0201 f00f ff00 ff00 ff00  .@ .............
	  ```
    * Output (viewed with command `xxd odu15-output-u32.bin`):
	  ```
      00000000: 8037 1301 7833 1100 1001 1100 1100 1100  .7..x3..........
	  ```
    * Additional Output (to console):
	  ``` 
      Read in 4 uint32_ts
      Found common divisor 15
	  ```

### u32-mcv
Usage:
	`u32-mcv <filename>`
* Finds the most common value in the given binary data.
* Input values of type uint32_t are provided in `<filename>`.
* Output of text summary is sent to stdout.
* Example ODU22 - A binary file is given as input with command `./u32-mcv odu22-input-u32.bin`: 
    * Input (viewed with command `xxd odu22-input-u32.bin`):
	  ```
      00000000: 0000 0000 aaaa aaaa 1111 1111 ffff ffff  ................
      00000010: ffff ffff ffff ffff 0000 0000 1111 1111  ................
      00000020: aaaa aaaa 3333 3333 3333 3333 3333 3333  ....333333333333
      00000030: 3333 3333 0200 0000 0300 0000 0100 0000  3333............
	  ```
    * Output (to console):
	  ``` 
      Read in 16 uint32_ts
      Encountered 8 distinct symbols.
      Most common symbol is 0x33333333 (count 4)
      p_hat = 0.25
      p_u = 0.53798647105856412
      minentropy = 0.89435820162615209
	  ```

### u32-regress-to-mean
Usage:
	`u32-regress-to-mean <filename> <k>`
* Calculate the mean, force each `k`-block to have this mean, and then round the resulting values.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `<k>`: Required. Integer value representing the number of samples in each block.
* Example ODU17 - A binary file is given as input, `<k>` is set to 3, and stdout is sent to a binary file with command `./u32-regress-to-mean odu17-input-u32.bin 3 > odu17-output-3-u32.bin`: 
    * Input (viewed with command `xxd odu17-input-u32.bin`):
	  ```
      00000000: 0100 0000 0200 0000 0300 0000 0400 0000  ................
      00000010: 0500 0000 0600 0000 0700 0000 0800 0000  ................
      00000020: 0900 0000 0a00 0000 0b00 0000 0c00 0000  ................
      ```
    * Output (viewed with command `xxd odu17-output-3-u32.bin`): 
	  ```
      00000000: 0600 0000 0700 0000 0800 0000 0600 0000  ................
      00000010: 0700 0000 0800 0000 0500 0000 0600 0000  ................
      00000020: 0700 0000 0500 0000 0600 0000 0700 0000  ................
	  ```
    * Additional Output (to console): 
	  ```
      Read in 12 uint32_ts
      Processing 4 blocks of 3 samples each.
      Global mean is 6.5.
      Adjusting block 0 by 5: Block mean: 2 (delta: 4.5) -> 7 (delta: 0.5)
      Adjusting block 1 by 2: Block mean: 5 (delta: 1.5) -> 7 (delta: 0.5)
      Adjusting block 2 by -2: Block mean: 8 (delta: 1.5) -> 6 (delta: 0.5)
      Adjusting block 3 by -5: Block mean: 11 (delta: 4.5) -> 6 (delta: 0.5)
	  ```

### u32-xor-diff
Usage:
	`u32-xor-diff`
* Produces the running XOR of adjacent values in provided binary data. 
* Input values of type uint32_t are provided via stdin.
* Output values of type uint32_t are sent to stdout.
* Example ODU20 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u32-xor-diff < odu20-input-u32.bin > odu20-output-u32.bin`: 
    * Input (viewed with command `xxd odu20-input-u32.bin`):
	  ```
      00000000: 0100 0000 0001 0000 0000 0100 0000 0001  ................
	  ```
    * Output (viewed with command `xxd odu20-output-u32.bin`):
	  ```
      00000000: 0101 0000 0001 0100 0000 0101            ............
	  ```