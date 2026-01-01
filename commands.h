#include "pico/stdlib.h"
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;


#define BLOCK_COUNT 4096
#define PAGE_COUNT 64
#define PAGE_SIZE 2112
#define CHUNK_SIZE 264

extern  u8 SHA[32];
extern int buffer_bytes;
extern  u8 pageBuffer[];

static inline void wait_8();
static inline void wait_16();
void init_gpio();


void send_cmd(u8 cmd);




u8 read_io();
void read_id();
void read_page(u32 addr);
void write_page(u32 addr, u8 *buf, size_t len, bool test, u8 test_val);
void erase_block(u32 addr);
void erase_chip();
void get_block_SHA(u32 addr);
void get_page_SHA(u32 addr);
void get_buffer_SHA();