#include "io.h"
#include "commands.h"
#include <stdio.h>
#define NUM_GPIO 8
const u32 io_mask = 0xff << 2;

#define IO_OUT() gpio_set_dir_masked(io_mask,io_mask);
#define IO_IN()  gpio_set_dir_masked(io_mask,0);


#define READ_ID 0x90
#define CACHE_READ 0x31
#define START_READ 0x00
#define START_PROGRAM 0x80
#define CONFIRM_PROGRAM 0x10
#define STATUS_REGISTER 0x70
#define START_ERASE 0x60
#define CONFIRM_ERASE 0xd0
#define READ1 0x30

#define PAGE_SIZE 2048 + 64


static u8 status = 0;




static inline void wait_8(){
    asm(
        "NOP \n\t"
    );
}
static inline void wait_16(){
    asm(
        "NOP \n\t"
        "NOP \n\t" 
    );
}

static inline void wait_100(){
    asm(
        "NOP \n\t"
        "NOP \n\t" 
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
        "NOP \n\t"        
    );
}

void hy_put(u8 gpio, u8 level) {
    gpio_put(gpio,level);
    wait_16();
   // sleep_us(1);
}

void init_gpio() {

        gpio_init_mask(io_mask);
        IO_OUT();

        gpio_init(RB);
        gpio_init(RE_);
        gpio_init(CE_);
        gpio_init(CLE);
        gpio_init(ALE);
        gpio_init(WE_);
        gpio_init(WP_);

        gpio_set_dir(RB, GPIO_IN);
        gpio_set_dir(RE_, GPIO_OUT);
        gpio_set_dir(CE_, GPIO_OUT);
        gpio_set_dir(CLE, GPIO_OUT);
        gpio_set_dir(ALE, GPIO_OUT);
        gpio_set_dir(WE_, GPIO_OUT);
        gpio_set_dir(WP_, GPIO_OUT);

        gpio_pull_up(RB);

        // default
        gpio_put(WE_,1);
        gpio_put(RE_,1);
        gpio_put(CE_,1);
        gpio_put(CLE,0);
        gpio_put(ALE,0);

        

}



void send_cmd(u8 cmd) {

    //for(int i = 0; i < NUM_GPIO; ++i) 
   //     gpio_put(io[i], (cmd >> i)&1);

   // gpio_set_mask((cmd << 2)&(0x3fc));
    gpio_put_masked(io_mask,(((u32)cmd)<<2));

    gpio_put(CLE,1);
    gpio_put(WE_,0);
    wait_16();
    wait_16();
    gpio_put(WE_,1);
    gpio_put(CLE,0);

    gpio_clr_mask(io_mask);

}



void cache_addr(u32 addr, u8 * frames) {

/*
    frames[0] = addr&0xff;
    frames[1] = (addr >> 8) & 0xf;
    frames[2] = (addr >> 12) & 0xff;
    frames[3] = (addr >> 20) & 0xff;
    frames[4] = (addr >> 28) & 0x3;
    */

    frames[0] = 0;
    frames[1] = 0;
    frames[2] = (addr)&0xff;
    frames[3] = (addr >> 8) & 0xff;
    frames[4] = (addr >> 16) & 0x3;

    //for(int i = 0; i < 5; ++i)
    //    printf("frame %x: %x\n",i,frames[i]);




}


static inline void send_addr(u8 * frames) {

    gpio_clr_mask(io_mask);

    for(int i = 0; i < 5; ++i) {
        gpio_put_masked(io_mask,(((u32)frames[i])<<2));
        gpio_put(WE_,0);
        wait_16();
        gpio_put(WE_,1);
        gpio_clr_mask(io_mask);
    }

}


static inline void send_row_addr(u8 * frames) {

    gpio_clr_mask(io_mask);

    for(int i = 2; i < 5; ++i) {
        gpio_put_masked(io_mask,(((u32)frames[i])<<2));
        gpio_put(WE_,0);
        wait_16();
        gpio_put(WE_,1);
        gpio_clr_mask(io_mask);
    }

}


static inline void send_data(u8 * frames, size_t len, bool test, u8 test_val) {
    
    gpio_clr_mask(io_mask);

    if(test)

        for(size_t i = 0; i < len; ++i) {
            gpio_put_masked(io_mask,(((u32)test_val)<<2));
            gpio_put(WE_,0);
            wait_16();
            gpio_put(WE_,1);
            gpio_clr_mask(io_mask);
        }

    else 
        for(size_t i = 0; i < len; ++i) {
            gpio_put_masked(io_mask,(((u32)frames[i])<<2));
            gpio_put(WE_,0);
            wait_16();
            gpio_put(WE_,1);
            gpio_clr_mask(io_mask);
        }

}


u8 read_io() {
    return (u8)((gpio_get_all() & io_mask) >> 2);
}

void read_id() {

    u32 id[4] = {0};

    gpio_put(CE_,0);
    IO_OUT();
    wait_16();
    send_cmd(READ_ID);

    hy_put(ALE,1);
    hy_put(WE_,0);

    gpio_put_masked(io_mask,0x0);
    wait_16();

    hy_put(WE_,1);
    hy_put(ALE,0);

    IO_IN();

    for(int i = 0; i < 4; ++i) {

        gpio_put(RE_,0);
        wait_16();

        id[i] = read_io();
        wait_16();

        gpio_put(RE_,1);
        wait_16();
}


    gpio_put(CE_,1);

    {
        int idx = 0;
        printf("ID: %x %x %x %x\nShould be: ad dc 10 95\n",id[idx++],id[idx++],id[idx++],id[idx++]);
    }

}


void read_status() {

    gpio_put(RE_,1);

    IO_OUT();

    send_cmd(STATUS_REGISTER);
    IO_IN();
    gpio_put(RE_,0);

    status = read_io();
    gpio_put(RE_,1);

}



void read_page(u32 addr) {

    gpio_put(RE_,1);
    gpio_put(ALE,0);
    gpio_put(CE_,0);
    IO_OUT();

   // printf("starting read...");

    u8 addr_frames[5] = {0};
    cache_addr(addr,addr_frames);

    send_cmd(START_READ);
    

    hy_put(ALE,1);
    send_addr(addr_frames);

    gpio_put(ALE,0);
    send_cmd(READ1);
    IO_IN();


    while(!gpio_get(RB));

    for(int i = 0; i < PAGE_SIZE; ++i) {
    
        gpio_put(RE_,0);
        wait_16();
        u8 ret = read_io();
        printf("%02x\n",ret);
        gpio_put(RE_,1);
        wait_16();

   }

    gpio_put(CE_,1);



}

void write_page(u32 addr, u8 *buf, size_t len, bool test, u8 test_val) {

    gpio_put(WP_,1);
    gpio_put(RE_,1);
    gpio_put(ALE,0);
    gpio_put(CE_,0);

    IO_OUT();

    u8 addr_frames[5] = {0};
    cache_addr(addr,addr_frames);

    send_cmd(START_PROGRAM);

    hy_put(ALE,1);
    send_addr(addr_frames);
    gpio_put(ALE,0);
    wait_100();

    send_data(buf,len,test,test_val);


    send_cmd(CONFIRM_PROGRAM);
    wait_100();

    printf("writing");
    while(!gpio_get(RB))
        printf(".");
    printf("done.\n");

    if(status&1)
        printf("WERR");


    gpio_put(CE_,1);

}

void erase_block(u32 addr) {


    gpio_put(WP_,1);
    gpio_put(RE_,1);
    gpio_put(ALE,0);
    gpio_put(CE_,0);
    IO_OUT();

    u8 addr_frames[5] = {0};
    cache_addr(addr,addr_frames);

    send_cmd(START_ERASE);

    hy_put(ALE,1);
    send_row_addr(addr_frames);
    gpio_put(ALE,0);

    send_cmd(CONFIRM_ERASE);
    wait_100();

    printf("erasing");
    while(!gpio_get(RB))
        printf(".");
    printf("done.\n");

    read_status();

    if(status&1)
        printf("EERR");


}


/*Once the program process starts, the Read Status Register command may be entered to read the status register.
The system controller can detect the completion of a program cycle by monitoring the R/B output, or the Status bit (I/
O 6) of the Status Register. Only the Read Status command and Reset command are valid while programming is in
progress. When the Page Program is complete, the Write Status Bit (I/O 0) may be checked. The internal write verify
detects only errors for "1"s that are not successfully programmed to "0"s. The command register remains in Read Sta-
tus command mode until another valid command is written to the command register.*/




/*
    GPIO numbering
    2, // 0
    3, // 1
    4,
    5,

    6,
    7,
    8,
    9, // 7
*/



