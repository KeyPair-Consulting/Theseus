# Theseus: Data Conversion Utilities

## Functions for word size and base input conversion:

### u8-to-u32
Usage:
	`u8-to-u32`
* Converts provided binary data from type uint8_t to type uint32_t.
* Input values of type uint8_t are provided via stdin.
* Output values of type uint32_t are sent to stdout.
* Example DCU14 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u8-to-u32 < dcu14-input-u8.bin > dcu14-output-u32.bin`: 
    * Input (viewed with command `xxd dcu14-input-u8.bin`):
	  ```
	  00000000: 4142 3031 6162 3839                      AB01ab89
      ```
    * Output (viewed with command `xxd dcu14-output-u32.bin`): 
	  ```
	  00000000: 4100 0000 4200 0000 3000 0000 3100 0000  A...B...0...1...
      00000010: 6100 0000 6200 0000 3800 0000 3900 0000  a...b...8...9...
	  ```

### u16-to-u32
Usage:
	`u16-to-u32 [-d]`
* Converts provided binary data from type uint16_t to type uint32_t.
* Input values of type uint16_t are provided via stdin.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-d`: Output differences between adjacent values.
* Example DCU04 - A binary file is given as input and stdout is sent to a binary file with command `./u16-to-u32 < dcu04-input-u16.bin > dcu04-output-u32.bin`: 
    * Input (viewed with command `xxd dcu04-input-u16.bin`):
	  ```
      00000000: 4142 3031 6162 3839 30                   AB01ab890
	  ```
    * Output (viewed with command `xxd dcu04-output-u32.bin`):
	  ```
	  00000000: 4142 0000 3031 0000 6162 0000 3839 0000  AB..01..ab..89..
	  ```
    * Alternate Output (if `-d` used, viewed with command `xxd dcu04-output-d-u32.bin`):
	  ```
	  00000000: efee 0000 3131 0000 d7d6 0000            ....11......
	  ```

### u64-to-u32
Usage:
	`u64-to-u32 [-r] [-t]`
* Converts provided binary data from type uint64_t to type uint32_t.
* Input values of type uint64_t are provided via stdin.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-r`: Switch endianness of input values.
    * `-t`: Truncate input values.
* Errors will occur if data remains out of range prior to uint32_t conversion.  I.e., the user must choose to truncate and/or switch endianness if necessary prior to type casting.
* Example DCU01 - A binary file is sent to stdin, truncation is selected, and stdout is sent to a binary file with command `./u64-to-u32 -t < dcu01-input-u64.bin > dcu01-output-u32.bin`: 
    * Input (viewed with command `xxd dcu01-input-u64.bin`):
	  ```
	  00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768 3839 30              abcdefgh890
	  ```
    * Output (viewed with command `xxd dcu01-output-u32.bin`):
	  ```
	  00000000: 4142 4344 3031 3233 6162 6364            ABCD0123abcd
	  ```

### dec-to-u32
Usage:
	`dec-to-u32`
* Converts provided human-readable decimal values to binary data.  (Note this is the opposite of u32-to-ascii.)
* Input values in decimal format are provided via stdin, one per line.
* Output values of type uint32_t are sent to stdout.
* Example DCU10 - A text file is sent to stdin and stdout is sent to a binary file with command `./dec-to-u32 < dcu10-input.txt > dcu10-output-u32.bin`: 
    * Input (in dcu10-input.txt, viewed in a text editor):
	  ```
	  1145258561
      1212630597
      858927408 
      926299444
      1684234849
      1751606885
	  ```
    * Output (viewed with command `xxd dcu10-output-u32.bin`): 
	  ```
	  00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768                      abcdefgh
	  ```

### dec-to-u64
Usage:
	`dec-to-u64`
* Converts provided human-readable decimal values to binary data.  (Note this is the opposite of u64-to-ascii.)
* Input values in decimal format are provided via stdin, one per line.
* Output values of type uint64_t are sent to stdout.
* Example DCU23 - A text file is sent to stdin and stdout is sent to a binary file with command `./dec-to-u64 < dcu23-input.txt > dcu23-output-u64.bin`: 
    * Input (in dcu23-input.txt, viewed in a text editor):
	  ```
      5208208757389214273
      3978425819141910832
      7523094288207667809
	  ```
    * Output (viewed with command `xxd dcu23-output-u64.bin`): 
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768                      abcdefgh
	  ```

### hex-to-u32
Usage:
	`hex-to-u32`
* Converts provided human-readable hexidecimal values to binary data.
* Input values in hexidecimal format are provided via stdin, spacing between bytes.
* Output values of type uint32_t are sent to stdout.
* Example DCU15 - A text file is sent to stdin and stdout is sent to a binary file with command `./hex-to-u32 < dcu15-input.txt > dcu15-output-u32.bin`: 
    * Input (in dcu15-input.txt, viewed with a text editor): 
	  ```
	  41 42 43 44 45 46 47 48 
      30 31 32 33 34 35 36 37
      61 62 63 64 65 66 67 68 
      38 39 30
	  ```
    * Output (viewed with command `xxd dcu15-output-u32.bin`): 
	  ```
	  00000000: 4100 0000 4200 0000 4300 0000 4400 0000  A...B...C...D...
      00000010: 4500 0000 4600 0000 4700 0000 4800 0000  E...F...G...H...
      00000020: 3000 0000 3100 0000 3200 0000 3300 0000  0...1...2...3...
      00000030: 3400 0000 3500 0000 3600 0000 3700 0000  4...5...6...7...
      00000040: 6100 0000 6200 0000 6300 0000 6400 0000  a...b...c...d...
      00000050: 6500 0000 6600 0000 6700 0000 6800 0000  e...f...g...h...
      00000060: 3800 0000 3900 0000 3000 0000            8...9...0...
	  ```

## Functions related to deltas, counters, endianness, and bit width:

### u32-delta
Usage:
	`u32-delta <filename>`
* Extracts deltas and then translates the result to positive values.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Example DCU18 - A binary file is given as input and stdout is sent to a binary file with command `./u32-delta dcu18-input-u32.bin > dcu18-output-u32.bin`: 
    * Input (viewed with command `xxd dcu18-input-u32.bin`):
	  ```
	  00000000: 1100 0000 2100 0000 4400 0000 5000 0000  ....!...D...P...
      00000010: 6600 0000 aa00 0000 ee00 0000 ff01 0000  f...............
      00000020: 0022 0000 0044 0000                      ."...D..
	  ```
    * Output (viewed with command `xxd dcu18-output-u32.bin`):
	  ```
      00000000: 0400 0000 1700 0000 0000 0000 0a00 0000  ................
      00000010: 3800 0000 3800 0000 0501 0000 f51f 0000  8...8...........
      00000020: f421 0000                                .!..
	  ```
	* Additional Output (to console):
	  ```
	  Read in 10 uint32_ts
      min diff: 12, max diff: 8704
	  ```

### u64-jent-to-delta
Usage:
	`u64-jent-to-delta`
* Converts provided binary data from uint64_t deltas in jent format (JEnt version 3.0.1 and earlier) to uint64_t deltas in nanoseconds format.  Also guesses native byte order and swaps if necessary.  Jent format expects the upper 32 bits to contain seconds and the lower 32 bits to contain nanoseconds.
* Input values of type uint64_t (in default jent delta format) are provided via stdin.
* Output values of type uint64_t are sent to stdout.
* Example DCU08 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u64-jent-to-delta < dcu08-input-u64.bin > dcu08-output-u64.bin`: 
    * Input (viewed with command `xxd dcu08-input-u64.bin`):
	  ```
      00000000: 0000 0000 0000 0000 00ca 9a3b 0000 0000  ...........;....
      00000010: 0000 0000 0100 0000 00ca 9a3b 0100 0000  ...........;....
      00000020: 0000 0000 0200 0000 0000 0000 0300 0000  ................
	  ```
    * Output (viewed with command `xxd dcu08-output-u64.bin`):
	  ```
      00000000: 0000 0000 0000 0000 00ca 9a3b 0000 0000  ...........;....
      00000010: 00ca 9a3b 0000 0000 0094 3577 0000 0000  ...;......5w....
      00000020: 0094 3577 0000 0000 005e d0b2 0000 0000  ..5w.....^......
	  ```
	* Additional Output (to console):
	  ```
      Native byte order seems better (0.5)
	  ```

### u32-counter-raw
Usage:
	`u32-counter-raw <filename>`
* Extracts deltas treated as 32-bit unsigned counters (they may roll over).
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Example DCU20 - A binary file is given as input and stdout is sent to a binary file with command `./u32-counter-raw dcu20-input-u32.bin > dcu20-output-u32.bin`: 
    * Input (viewed with command `xxd dcu20-input-u32.bin`):
	  ```
      00000000: ffff 0001 ffff 0011 ffff ff13 ffff ff21  ...............!
      00000010: ffff ff66 ffff ff88 ffff ffaa ffff ffff  ...f............
      00000020: 1100 0000 2100 0000                      ....!...
	  ```
    * Output (viewed with command `xxd dcu20-output-u32.bin`):
	  ```
      00000000: f0ff ff0f f0ff fe02 f0ff ff0d f0ff ff44  ...............D
      00000010: f0ff ff21 f0ff ff21 f0ff ff54 0200 0000  ...!...!...T....
      00000020: 0000 0000                                ....
	  ```
	* Additional Output (to console):
	  ```
	  Read in 10 uint32_ts
      Shifting data down by 16. Maximum value now 1426063344
	  ```

### u64-counter-raw
Usage:
	`u64-counter-raw <filename>`
* Extracts deltas treated as 64-bit unsigned counters (they may roll over).
* Input values of type uint64_t are provided in `<filename>`.
* Output values of type uint64_t are sent to stdout.
* Example DCU02 - A binary file is given as input and stdout is sent to a binary file with command `./u64-counter-raw dcu02-input-u64.bin > dcu02-output-u64.bin`: 
    * Input (viewed with command `xxd dcu02-input-u64.bin`):
	  ```
      00000000: 0100 0000 0000 0000 0400 0000 0000 0000  ................
      00000010: 1100 0000 0000 0000 2100 0000 0000 0000  ........!.......
      00000020: ffff ffff ffff ffff 0300 0000 0000 0000  ................
	  ```
    * Output (viewed with command `xxd dcu02-output-u64.bin`):
	  ```
      00000000: 0000 0000 0000 0000 0a00 0000 0000 0000  ................
      00000010: 0d00 0000 0000 0000 dbff ffff ffff ffff  ................
      00000020: 0100 0000 0000 0000                      ........
	  ```
	* Additional Output (to console):
	  ```
	  Read in 6 uint64_ts
      Shifting data down by 3. Maximum value now 18446744073709551579
	  ```

### u32-counter-endian
Usage:
	`u32-counter-endian [-d] <filename>`
* Trys to guess counter endianness and translates to the local platform.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Options:
    * `-d`: Output differences between adjacent values (when viewing the data as a 32-bit unsigned counter with rollover).
* Example DCU17 - A binary file is given as input and stdout is sent to a binary file with command `./u32-counter-endian dcu17-input-u32.bin > dcu17-output-u32.bin`: 
    * Input (viewed with command `xxd dcu17-input-u32.bin`):
	  ```
      00000000: 0000 0011 0000 0021 0000 0044 0000 0050  .......!...D...P
      00000010: 0000 0066 0000 00aa 0000 00ee 0000 01ff  ...f............
      00000020: 0000 2200 0000 4400                      .."...D.
	  ```
    * Output (viewed with command `xxd dcu17-output-u32.bin`):
	  ```
      00000000: 1100 0000 2100 0000 4400 0000 5000 0000  ....!...D...P...
      00000010: 6600 0000 aa00 0000 ee00 0000 ff01 0000  f...............
      00000020: 0022 0000 0044 0000                      ."...D..
	  ```
    * Alternate Output (if `-d` used, viewed with command `xxd dcu17-output-d-u32.bin`):
	  ```
	  00000000: 1000 0000 2300 0000 0c00 0000 1600 0000  ....#...........
      00000010: 4400 0000 4400 0000 1101 0000 0120 0000  D...D........ ..
      00000020: 0022 0000                                ."..
	  ```
	* Additional Output (to console, in both Output cases above):
	  ```
      Read in 10 uint32_ts
      Reversed format detected (0.90000000000000002 vs 0.80000000000000004)
	  ```

### u64-counter-endian
Usage:
	`u64-counter-endian`
* Trys to guess counter endianness and translates to the local platform.
* Input values of type uint64_t are provided via stdin.
* Output values of type uint64_t are sent to stdout.
* Example DCU21 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u64-counter-endian < dcu21-input-u64.bin > dcu21-output-u64.bin`: 
    * Input (viewed with command `xxd dcu21-input-u64.bin`):
	  ```
	  00000000: 0000 0000 0000 0011 0000 0000 0000 0021  ...............!
      00000010: 0000 0000 0000 0044 0000 0000 0000 0050  .......D.......P
      00000020: 0000 0000 0000 0066                      .......f
	  ```
    * Output (viewed with command `xxd dcu21-output-u64.bin`):
	  ```
	  00000000: 1100 0000 0000 0000 2100 0000 0000 0000  ........!.......
      00000010: 4400 0000 0000 0000 5000 0000 0000 0000  D.......P.......
      00000020: 6600 0000 0000 0000                      f.......
	  ```
	* Additional Output (to console):
	  ```
	  Swapped byte order seems better (1)
	  ```

### u64-change-endianness
Usage:
	`u64-change-endianness`
* Changes between big and little endian byte-ordering conventions.
* Input values of type uint64_t are provided via stdin.
* Output values of type uint64_t are sent to stdout.
* Example DCU22 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u64-change-endianness < dcu22-input-u64.bin > dcu22-output-u64.bin`: 
    * Input (viewed with command `xxd dcu22-input-u64.bin`):
	  ```
	  00000000: 0000 0000 0000 0011 0000 0000 0000 0021  ...............!
      00000010: 0000 0000 0000 0044 0000 0000 0000 0050  .......D.......P
      00000020: 0000 0000 0000 0066                      .......f
	  ```
    * Output (viewed with command `xxd dcu22-output-u64.bin`):
	  ```
	  00000000: 1100 0000 0000 0000 2100 0000 0000 0000  ........!.......
      00000010: 4400 0000 0000 0000 5000 0000 0000 0000  D.......P.......
      00000020: 6600 0000 0000 0000                      f.......
	  ```

### u32-counter-bitwidth
Usage:
	`u32-counter-bitwidth <filename>`
* Extracts deltas under the assumption that the data is an increasing counter of some inferred bitwidth.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint32_t are sent to stdout.
* Example DCU19 - A binary file is given as input and stdout is sent to a binary file with command `./u32-counter-bitwidth dcu19-input-u32.bin > dcu19-output-u32.bin`: 
    * Input (viewed with command `xxd dcu19-input-u32.bin`):
	  ```
      00000000: ffff 0001 ffff 0011 ffff ff13 ffff ff21  ...............!
      00000010: ffff ff66 ffff ff88 ffff ffaa ffff ffff  ...f............
	  ```
    * Output (viewed with command `xxd dcu19-output-u32.bin`):
	  ```
      00000000: 0000 010d 0000 0000 0000 010b 0000 0142  ...............B
      00000010: 0000 011f 0000 011f 0000 0152            ...........R
	  ```
	* Additional Output (to console):
	  ```
      Read in 8 uint32_ts
      Next binary power: 0 (assuming a 32 bit counter)
      min diff: 50266112, max diff: 1426063360
      Translating min diff to 0 (new max diff: 1375797248)
	  ```

### u32-expand-bitwidth
Usage:
	`u32-expand-bitwidth <filename>`
* Extracts inferred values under the assumption that the data is a truncation of some sampled value, whose bitwidth is inferred.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type uint64_t are sent to stdout.
* Example DCU11 - A binary file is given as input and stdout is sent to a binary file with command `./u32-expand-bitwidth dcu11-input-u32.bin > dcu11-output-u64.bin`: 
    * Input (viewed with command `xxd dcu11-input-u32.bin`):
	  ```
      00000000: 0000 010d 0000 0000 0000 010b 0000 0142  ...............B
      00000010: 0000 011f 0000 011f 0000 0152            ...........R
	  ```
    * Output (viewed with command `xxd dcu11-output-u64.bin`):
	  ```
	  00000000: 0000 010d 0000 0000 0000 0000 0000 0000  ................
      00000010: 0000 010b 0000 0000 0000 0142 0000 0000  ...........B....
      00000020: 0000 011f 0000 0000 0000 011f 0000 0000  ................
      00000030: 0000 0152 0000 0000                      ...R....
	  ```
	* Additional Output (to console):
	  ``` 
	  Read in 7 uint32_ts
      Next binary power: 2147483648 (assuming a 31 bit value)
      Found most obvious transition at index: 3 (0x0B010000 -> 0x42010000)
      Now 000b010000 -> 0x0042010000
      New max 1375797248 (index 6)
      New min 0
	  ```

## Functions for converting to type statData_t for statistical analysis:

### u8-to-sd
Usage:
	`u8-to-sd [-l] [-v] <bits per symbol; 1, 2, 4>`
* Converts provided binary data from type uint8_t to type statData_t.
* Input values of type uint8_t are provided via stdin.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-l`: Bytes should be output low bits to high bits.
    * `-v`: Increase verbosity. Can be used multiple times.
	* `<bits per symbol>`: Number of bits per symbol, limited to values 1, 2, or 4.
* Example DCU03 - A binary file is sent to stdin, -l with 1 bit/symbol is selected, and stdout is sent to a binary file with command `./u8-to-sd -l 1 < dcu03-input-u8.bin > dcu03-output-1-sd.bin`: 
    * Input (viewed with command `xxd dcu03-input-u8.bin`):
	  ```
      00000000: 4142 3031 6162 3839                      AB01ab89
	  ```
    * Output (viewed with command `xxd dcu03-output-1-sd.bin`):
	  ```
      00000000: 0100 0000 0000 0100 0001 0000 0000 0100  ................
      00000010: 0000 0000 0101 0000 0100 0000 0101 0000  ................
      00000020: 0100 0000 0001 0100 0001 0000 0001 0100  ................
      00000030: 0000 0001 0101 0000 0100 0001 0101 0000  ................
	  ```
	* Alternate Output (if bits per symbol = 2, viewed with command `xxd dcu03-output-2-sd.bin`)
	  ```
      00000000: 0100 0001 0200 0001 0000 0300 0100 0300  ................
      00000010: 0100 0201 0200 0201 0002 0300 0102 0300  ................
	  ```
	* Alternate Output (if bits per symbol = 4, viewed with command `xxd dcu03-output-4-sd.bin`)
	  ```
      00000000: 0104 0204 0003 0103 0106 0206 0803 0903  ................
	  ```

### u16-to-sdbin
Usage:
	`u16-to-sdbin [-l] [-b]`
* Converts provided binary data from type uint16_t to type statData_t by expanding packed bits.
* Input values of type uint16_t are provided via stdin.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Options:
    * `-l`: Extract bits from low bit to high bit.
	* `-b`: Input values are in big endian format.
* Example DCU07 - A binary file is sent to stdin and stdout is sent to a binary file with command `./u16-to-sdbin < dcu07-input-u16.bin > dcu07-output-sd.bin`: 
    * Input (viewed with command `xxd dcu07-input-u16.bin`):
	  ```
      00000000: 0101 1111 1010 00ff ff00 0000            ............
	  ```
    * Output (viewed with command `xxd dcu07-output-sd.bin`):
	  ```
      00000000: 0000 0000 0000 0001 0000 0000 0000 0001  ................
      00000010: 0000 0001 0000 0001 0000 0001 0000 0001  ................
      00000020: 0000 0001 0000 0000 0000 0001 0000 0000  ................
      00000030: 0101 0101 0101 0101 0000 0000 0000 0000  ................
      00000040: 0000 0000 0000 0000 0101 0101 0101 0101  ................
      00000050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
	  ```
    * Alternate Output (if `-l` used, viewed with command `xxd dcu07-output-l-sd.bin`):
	  ```
      00000000: 0100 0000 0000 0000 0100 0000 0000 0000  ................
      00000010: 0100 0000 0100 0000 0100 0000 0100 0000  ................
      00000020: 0000 0000 0100 0000 0000 0000 0100 0000  ................
      00000030: 0000 0000 0000 0000 0101 0101 0101 0101  ................
      00000040: 0101 0101 0101 0101 0000 0000 0000 0000  ................
      00000050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
	  ```
    * Alternate Output (if `-b` used, viewed with command `xxd dcu07-output-b-sd.bin`):
	  ```
      00000000: 0000 0000 0000 0001 0000 0000 0000 0001  ................
      00000010: 0000 0001 0000 0001 0000 0001 0000 0001  ................
      00000020: 0000 0001 0000 0000 0000 0001 0000 0000  ................
      00000030: 0000 0000 0000 0000 0101 0101 0101 0101  ................
      00000040: 0101 0101 0101 0101 0000 0000 0000 0000  ................
      00000050: 0000 0000 0000 0000 0000 0000 0000 0000  ................
	  ```

### u32-to-sd
Usage:
	`u32-to-sd <filename>`
* Converts provided binary data from type uint32_t to type statData_t.
* Input values of type uint32_t are provided in `<filename>`.
* Output values of type statData_t (default uint8_t) are sent to stdout.
* Example DCU06 - A binary file is given as input and stdout is sent to a binary file with command `./u32-to-sd dcu06-input-u32.bin > dcu06-output-sd.bin`: 
    * Input (viewed with command `xxd dcu06-input-u32.bin`):
	  ```
	  00000000: 4100 0000 4200 0000 3000 0000 3100 0000  A...B...0...1...
      00000010: 6100 0000 6200 0000 3800 0000 3900 0000  a...b...8...9...
      00000020: 3000                                     0.
      ```
    * Output (viewed with command `xxd dcu06-output-sd.bin`): 
	  ```
      00000000: 4142 3031 6162 3839                      AB01ab89
	  ```

### blocks-to-sdbin
Usage:
	`blocks-to-sdbin [-l] <blocksize> <ordering>`
* Extracts bits from a blocksize-sized block one byte at a time, in the specified byte ordering.
* Input values in string format are provided via stdin.   
* Output values of type statData_t (default uint8_t) are sent to stdout.  Each output represents a single bit.
* Options:
    * `-l`: Extract bits from least to most significant within each byte.
	* `<blocksize>`: Required. The number of bytes per block.
    * `<ordering>`: Required. The indexing order for bytes (zero indexed decimal, separated by colons).  E.g., `0:1:2:3:4`.
* Example DCU16 - A binary file is sent to stdin (stored in 3 byte blocks ordered from left to right) and stdout is sent to a binary file with command `./blocks-to-sdbin 3 0:1:2 < dcu16-input-3byteblocks.bin > dcu16-output-sd.bin`: 
    * Input (viewed with command `xxd dcu16-input-3byteblocks.bin`):
	  ```
      00000000: 0011 22aa bbcc 3344 55dd eeff 6677 88    .."...3DU...fw.
	  ```
    * Output (viewed with command `xxd dcu16-output-sd.bin`):
	  ```
      00000000: 0000 0000 0000 0000 0000 0001 0000 0001  ................
      00000010: 0000 0100 0000 0100 0100 0100 0100 0100  ................
      00000020: 0100 0101 0100 0101 0101 0000 0101 0000  ................
      00000030: 0000 0101 0000 0101 0001 0000 0001 0000  ................
      00000040: 0001 0001 0001 0001 0101 0001 0101 0001  ................
      00000050: 0101 0100 0101 0100 0101 0101 0101 0101  ................
      00000060: 0001 0100 0001 0100 0001 0101 0001 0101  ................
      00000070: 0100 0000 0100 0000                      ........
	  ```
    * Alternate Output (if `-l` used, viewed with command `xxd dcu16-output-l-sd.bin`):
	  ```
      00000000: 0000 0000 0000 0000 0100 0000 0100 0000  ................
      00000010: 0001 0000 0001 0000 0001 0001 0001 0001  ................
      00000020: 0101 0001 0101 0001 0000 0101 0000 0101  ................
      00000030: 0101 0000 0101 0000 0000 0100 0000 0100  ................
      00000040: 0100 0100 0100 0100 0100 0101 0100 0101  ................
      00000050: 0001 0101 0001 0101 0101 0101 0101 0101  ................
      00000060: 0001 0100 0001 0100 0101 0100 0101 0100  ................
      00000070: 0000 0001 0000 0001                      ........
	  ```

## Functions for output conversion (for histograms, human readability, categorization, etc.):

### sd-to-hex
Usage:
	`sd-to-hex`
* Converts provided binary data to human-readable hexidecimal values.
* Input values of type statData_t (default uint8_t) are provided via stdin.
* Output values in hexidecimal format are sent to stdout, one per line.
* Example DCU13 - A binary file is sent to stdin with command `./sd-to-hex < dcu13-input-u8.bin`: 
    * Input (viewed with command `xxd dcu13-input-u8.bin`):
	  ```
	  00000000: 4142 3031 6162 3839                      AB01ab89
	  ```
    * Output (to console): 
	  ```
	  41 
	  42 
	  30 
	  31 
	  61 
	  62 
	  38 
	  39 
	  ```

### u32-to-ascii
Usage:
	`u32-to-ascii`
* Converts provided binary data to human-readable decimal values.  (Note this is the opposite of dec-to-u32.)
* Input values of type uint32_t are provided via stdin.
* Output values are sent to stdout, one per line.
* Example DCU12 - A binary file is sent to stdin with command `./u32-to-ascii < dcu12-input-u32.bin`: 
    * Input (viewed with command `xxd dcu12-input-u32.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768 3839 30              abcdefgh890
	  ```
    * Output (to console): 
	  ```
      1145258561
      1212630597
      858927408 
      926299444 
      1684234849
      1751606885
	  ```

### u64-to-ascii
Usage:
	`u64-to-ascii`
* Converts provided binary data to human-readable decimal values.
* Input values of type uint64_t are provided via stdin.
* Output values in decimal format are sent to stdout, one per line.
* Example DCU05 - A binary file is sent to stdin with command `./u64-to-ascii < dcu05-input-u64.bin`: 
    * Input (viewed with command `xxd dcu05-input-u64.bin`):
	  ```
      00000000: 4142 4344 4546 4748 3031 3233 3435 3637  ABCDEFGH01234567
      00000010: 6162 6364 6566 6768 3839 30              abcdefgh890
	  ```
    * Output (to console): 
	  ```
      5208208757389214273
      3978425819141910832
      7523094288207667809
	  ```

### u32-to-categorical
Usage:
	`u32-to-categorical [-v] [-m] [-t <value>] [-z] <filename>`
* Produces categorical summary of provided binary data.
* Input values of type uint32_t are provided in `<filename>`.   
* Output of text summary is sent to stdout.
* Options:
    * `-v`: Increase verbosity. Can be used multiple times.
    * `-m`: Output in Mathematica-friendly format.
    * `-t <value>`: Trim any value that is prior to the first symbol with `<value>` occurrences or more or after the last symbol with `<value>` occurrences or more.
    * `-z`: Don't output zero categories.
* Example DCU09 - A binary file is given as input with command `./u32-to-categorical dcu09-input-u32.bin`: 
    * Input (viewed with command `xxd dcu09-input-u32.bin`):
	  ```
      00000000: 0100 0000 0100 0000 0200 0000 0500 0000  ................
      00000010: 0500 0000 0500 0000 0500 0000 0100 0000  ................
	  ```
    * Output (to console):
	  ```
      1: 3
      2: 1
      3: 0
      4: 0
      5: 4
	  ```
    * Alternate Output (if `-m` used, to console):
	  ```
	  ./dcu09-input-u32.bin={{1, 3},{2, 1},{3, 0},{4, 0},{5, 4}};
	  ```
    * Alternate Output (if `-t 4` used, to console):
	  ```
      5: 4
	  ```
	* Alternate Output (if `-z` used, to console):
	  ```
      1: 3
      2: 1
      5: 4
	  ```