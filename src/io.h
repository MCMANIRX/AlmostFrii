#include <stdint.h>
#pragma once

typedef uint8_t u8;
typedef uint32_t u32;


const u8 io[8] = {
    2,
    3,
    4,
    5,

    6,
    7,
    8,
    9,
};

const u8 RB = 11;
const u8 RE_ = 12;
const u8 CE_ = 13;

const u8 CLE = 18;
const u8 ALE = 19;
const u8 WE_ = 20;
const u8 WP_ = 21;

// wp must be high for write or erase 

/* Addresses are accepted with Chip Enable low, 
Address Latch Enable High, 
Command Latch Enable low and 
Read Enable high 
and latched on the rising edge of Write Enable*/
