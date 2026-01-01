#include "io.h"
#include "commands.h"
#include <stdio.h>
#include "mbedtls/sha256.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "read_io_sm.pio.h"


#define NUM_GPIO 8
const u32 io_mask = (0xff << 2);




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



u8 SHA[32] = {0};
/* internal ptr to str buf */
u8 pageBuffer[PAGE_SIZE] = {0};
 int buffer_bytes=0;


static u8 status = 0;


static    PIO pio;
static    uint offset;
static    uint sm ;

static     int rx_dma;
static     int reset_rx_dma;




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


void gpio_to_pio() {

    for(int i = 0; i < NUM_GPIO; ++i){
        gpio_set_dir(io[i],GPIO_OUT);
        gpio_put(io[i],0);
        pio_gpio_init(pio, io[i]);            
        gpio_set_function(io[i], GPIO_FUNC_PIO0);
    }
        pio_sm_set_consecutive_pindirs(pio,sm,io[0],NUM_GPIO,false);

        gpio_set_dir(RE_,GPIO_OUT);
        gpio_put(RE_,1);
        pio_gpio_init(pio, RE_);            
        gpio_set_function(RE_, GPIO_FUNC_PIO0);
        pio_sm_set_consecutive_pindirs(pio,sm,RE_,1,true);

    pio_sm_set_enabled(pio, sm, true);

}

void pio_to_gpio() {
    pio_sm_set_enabled(pio, sm, false);

    for(int i = 0; i < NUM_GPIO; ++i){
        gpio_init(io[i]);
        gpio_set_dir(io[i],GPIO_OUT);
        gpio_put(io[i],0);
    }
    
    gpio_init(RE_);
    gpio_set_dir(RE_,GPIO_OUT);
    gpio_put(RE_,1);

}

/*
static volatile bool buffer_full = false;

void pio0_irq0_handler(void) {

    pio_interrupt_clear(pio0, 0); // clears IRQ0 so PIO can continue
    buffer_full = true;

}*/

void reset_dma(){

    dma_channel_abort(rx_dma);               // stop current transfer
    dma_channel_set_write_addr(rx_dma, &pageBuffer, true);
    dma_channel_set_transfer_count(rx_dma, PAGE_SIZE, true);
    dma_channel_start(rx_dma);               // restart transfer

}


// setup read state machine and DMA
void init_pio_dma() {

    pio = pio0;
    offset = pio_add_program(pio, &read_io_sm_program);
    sm = pio_claim_unused_sm(pio, true);
    read_io_sm_init(pio, sm, offset, io[0],NUM_GPIO,io_mask,RE_);
    pio_sm_put(pio,sm,PAGE_SIZE-1);

    //pio_set_irq0_source_enabled(pio0, pis_interrupt0, true);
    pio_set_irq1_source_enabled(pio0, pis_interrupt1, true);

  //  irq_set_exclusive_handler(PIO0_IRQ_0, pio0_irq0_handler);
  //  irq_set_enabled(PIO0_IRQ_0, true);

    

    //static uintptr_t bufferPtr = (uintptr_t)pageBuffer;

    rx_dma = dma_claim_unused_channel(true);
    reset_rx_dma = dma_claim_unused_channel(true);

    dma_channel_config c0 = dma_channel_get_default_config(rx_dma);

    channel_config_set_transfer_data_size(&c0,DMA_SIZE_8);
    channel_config_set_read_increment(&c0,false);
    channel_config_set_write_increment(&c0,true);

    channel_config_set_dreq(&c0, pio_get_dreq(pio, sm, false)) ;  // Pace the FIFO to the writing of data
    //channel_config_set_chain_to(&c0, reset_rx_dma);                         // chain to other channel

    dma_channel_configure(
        rx_dma,                // Channel to be configured
        &c0,          
        &pageBuffer,           
        &pio->rxf[sm], 
        PAGE_SIZE,     
        false                        // Start immediately.
    ); 

    /*

    // Channel One (reconfigures the first channel)
    dma_channel_config c1 = dma_channel_get_default_config(reset_rx_dma);   // default configs
    channel_config_set_transfer_data_size(&c1, DMA_SIZE_32);              // 32-bit txfers
    channel_config_set_read_increment(&c1, false);                        // no read incrementing
    channel_config_set_write_increment(&c1, false);                       // no write incrementing
    channel_config_set_chain_to(&c1, rx_dma);                         // chain to other channel

    dma_channel_configure(
        reset_rx_dma,                         // Channel to be configured
        &c1,                                // The configuration we just created
        &dma_hw->ch[rx_dma].write_addr,  // Write address (channel 0 read address)
        &bufferPtr,                   // Read address (POINTER TO AN ADDRESS)
        1,                                  // Number of transfers, in this case each is 4 byte
        false                               // Don't start immediately.
    );
    */

    dma_channel_start(rx_dma);
    pio_to_gpio();






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


        init_pio_dma();
  

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

   /* for(int i = 0; i < 4; i++)
        printf("frame %x: %x\n",i,frames[i]);*/




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
u8 *test_string = "quickbrownfox";
u8 idx = 0;
static inline void send_data(u8 * frames, size_t len, bool test, u8 test_val) {
    
    gpio_clr_mask(io_mask);

    if(test)

        for(size_t i = 0; i < len; ++i) {
            gpio_put_masked(io_mask,(((u32)test_string[idx++])<<2));
            idx%=13;
            gpio_put(WE_,0);
            wait_100();
            gpio_put(WE_,1);
            gpio_clr_mask(io_mask);
        }

    else 
        for(size_t i = 0; i < len; ++i) {
            gpio_put_masked(io_mask,(((u32)frames[i])<<2));
            gpio_put(WE_,0);
            wait_100();
            gpio_put(WE_,1);
            gpio_clr_mask(io_mask);
        }

}


u8 read_io() {
    return (u8)((gpio_get_all() & io_mask) >> 2);
}

void read_id() {
    gpio_put(WP_,0);

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

    /*gpio_to_pio();

    pio_sm_put(pio,sm,PAGE_SIZE);
    dma_channel_wait_for_finish_blocking(rx_dma);

    pio_to_gpio();*/

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
    gpio_put(WP_,0);

    gpio_put(RE_,1);

    IO_OUT();

    send_cmd(STATUS_REGISTER);
    IO_IN();
    gpio_put(RE_,0);

    status = read_io();
    gpio_put(RE_,1);

}



void read_page(u32 addr) {
    gpio_put(WP_,0);

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
    gpio_to_pio();
    // IO_IN();

    while(!gpio_get(RB));
    wait_16();

   /*/ for(int i = 0; i < PAGE_SIZE; ++i) {
        gpio_put(RE_,0);
        wait_8();
        pageBuffer[i] = read_io();
        gpio_put(RE_,1);
   }*/

    pio_interrupt_clear(pio0, 1); // clears IRQ1 so PIO can continue
    dma_channel_wait_for_finish_blocking(rx_dma);


    //while(!buffer_full);
   // buffer_full = false;

    pio_to_gpio();

    gpio_put(CE_,1);

    // TODO:: actually use tusb

    for(int i =0; i  < 8; ++i) {
        for(int j = 0; j < 264; ++j)
            printf("%02x ",pageBuffer[j + (264*i)]);
        printf("\n");
   }

    reset_dma();





}
#include "pico/cyw43_arch.h"

void write_page(u32 addr, u8 *buf, size_t len, bool test, u8 test_val) {
    //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

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

    send_data(buf,PAGE_SIZE,test,test_val);


    send_cmd(CONFIRM_PROGRAM);
    wait_100();

    //puts("writing");
    while(!gpio_get(RB));
    //    puts(".");
    //puts("done.\n");

    if(status&1)
        puts("WERR");


    gpio_put(CE_,1);
    gpio_put(WP_,0);
    //cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

}

void erase_block(u32 addr) {
    addr<<=6;


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
        puts("EERR");

    gpio_put(WP_,0);

}

void erase_chip() {

    for(int i = 0; i < 0x1000; ++i){
        printf("block %d: ",i);
        erase_block(i&0xfff);
    }
    printf("erase complete.");
}


void read_page_SHA(u32 addr) {

    gpio_put(WP_,0);
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
    gpio_to_pio();
    // IO_IN();

    while(!gpio_get(RB));
    wait_16();

    pio_interrupt_clear(pio0, 1); // clears IRQ1 so PIO can continue
    dma_channel_wait_for_finish_blocking(rx_dma);

    pio_to_gpio();

    gpio_put(CE_,1);

    reset_dma();





}

void get_block_SHA(u32 addr) {
    addr<<=6;
    u32 vaddr = addr;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx,false);

    for(int i = 0; i < 0x40; ++i){
        read_page_SHA(vaddr++);
        mbedtls_sha256_update(&ctx,pageBuffer,PAGE_SIZE);
    }
        mbedtls_sha256_finish(&ctx,SHA);


    // output
    printf("SHA: ");
    for(int i =0; i < 32; ++i)
        printf("%02x",SHA[i]);
    //printf("\nblock:%x (row: 0x%x->0x%x) (linear: 0x%x->0x%x)",addr,addr,vaddr,addr*PAGE_SIZE,vaddr*PAGE_SIZE);
    printf("\n");


}

void get_page_SHA(u32 addr) {
    u32 vaddr = addr;
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx,false);

    read_page_SHA(vaddr++);
    mbedtls_sha256_update(&ctx,pageBuffer,PAGE_SIZE);
    mbedtls_sha256_finish(&ctx,SHA);

    // output
    printf("SHA: ");
    for(int i =0; i < 32; ++i)
        printf("%02x",SHA[i]);
    //printf("\nblock:%x (row: 0x%x->0x%x) (linear: 0x%x->0x%x)",addr,addr,vaddr,addr*PAGE_SIZE,vaddr*PAGE_SIZE);
    printf("\n");


}


void get_buffer_SHA() {

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx,false);

    mbedtls_sha256_update(&ctx,pageBuffer,PAGE_SIZE);
    mbedtls_sha256_finish(&ctx,SHA);

    // output
    printf("SHA: ");
    for(int i =0; i < 32; ++i)
        printf("%02x",SHA[i]);
    //printf("\nblock:%x (row: 0x%x->0x%x) (linear: 0x%x->0x%x)",addr,addr,vaddr,addr*PAGE_SIZE,vaddr*PAGE_SIZE);
    printf("\n");


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




