# Pico-NAND-Driver

Raspberry Pi Pico controller for Nintendo Wii NAND, HY27UF084G2B/M 512MByte parallel NAND flash. This project currently only supports 1:1 NAND flashing i.e. no ECC calculation.

## Features

- Read/Write individual pages by row address (0x00->0x03FFFF)

- Read manufacturer code

- Block and full chip erase

- On-chip SHA256 calculation for individual pages or blocks

- Logging for NAND dumps and programs

## Notes

- Page size: **2KB** (2048 valid data +64 ECC bytes, so **2112 bytes** in total)

- Block Size: 64 Pages = **128KB** (**135168 bytes total**) 
- Block count: **4096**

## Usage

### Python Scripts

- **nand_dump.py**: automatically dumps entire chip to `nand_dumps/nand-{epoch}.bin`. Takes ~20min.

- **program.py**: programs NAND chip from file input and computes SHA256 per-block and per-page. takes ~1.5hrs.

## `nand_dump.py` Arguments

- `P` sets the port to use

- `l` sets the **l**ength of blocks to read (default 4096)

- `a` sets the block **a**ddress **in hex** to begin reading (default 0)

- `d` enables command **d**ebug mode, quitting program before NAND interfacing begins

## `nand_dump.py` Examples

### Example 1
 ```bash
 python ./nand_dump.py -P"/dev/tty.usbmodem101" -l800 -a800
 ```

- Dumps 800 pages of NAND starting from block address 0x800 (2048) using `/dev/tty.usbmodem101`

**output:**

```bash
Reading 800 blocks from start address 2048 with port COM13.
```

### Example 2
 ```bash
 python ./nand_dump.py -P"/dev/tty.usbmodem101"
 ``` 

- Dumps entire NAND using `/dev/tty.usbmodem101`

**output:**

```bash
Reading 4096 blocks from start address 0 with port COM13.
```

## `nand_program.py` Arguments

- `w` enables write mode

- `t` enables **t**est write mode ("writes pattern `0xAD`)

- `p` enables per-**p**age SH256A calculation
- `P` sets the port to use

- `b` enables per-**b**lock SHA256 calculation (enabled by default)

    - May be disabled with `B`

- `l` sets the **l**ength of write in blocks (default 0)

- `a` sets the block **a**ddress **in hex** to begin writing (default 0)

- `d` enables command **d**ebug mode, quitting program before NAND interfacing begins

## `nand_program.py` Examples

### Example 1
 ```bash
 python ./nand_program.py ./path-to-file -wpbd -l60 -aA8 -P"/dev/tty.usbmodem101"
 ```  

- Starting from the address of block `0xA8`, will write 60 blocks to NAND using port `/dev/tty.usbmodem101` provided file and computes per-block and per-page SHA256, but exits program after printing debug message.

**output:**

```bash
Doing write from image ./path-to-file with per-page and per-block SHA256 calculation, starting at block address 0xa8 for 60 blocks
```
\*program exits\*