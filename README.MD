# Pico-NAND-Driver
Raspberry Pi Pico controller for HY27UF084G2B/M 512MByte parallel NAND flash. This project currently only supports 1:1 NAND flashing i.e. no ECC calculation.

## Features
- Read/Write individual pages by row address (0x00->0x03FFFF)
- Read manufacturer code
- Block and full chip erase
- On-chip SHA256 calculation for individual pages or blocks

## Notes
- Page size: **2KB** (+64 ECC bytes, so **2112 bytes** in total)
- Block Size: 64 Pages = **128KB** (**135168 bytes total**) 
- Block count: **4096**

## Usage
### Python Scripts
- **dump.py**: automatically dumps entire chip to `nand.bin`. Takes approx. >10hrs.
- **program.py**: programs NAND chip from file input and computes SHA256 per-block
### Raw Usage
TODO