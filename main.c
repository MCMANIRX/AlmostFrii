#include <stdio.h>
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>
#include "pico/cyw43_arch.h"

#include "commands.h"

#define BLOCK_COUNT 4096
#define PAGE_COUNT 64
#define PAGE_SIZE 2112


u8 str[PAGE_SIZE + 0x40] = {0}; // bidirectional string and page buffer
u8 str_idx = 0;


typedef struct {
    char txt[24];
    int len;
} MARKER;


static void init_marker(MARKER * marker, char *str) {
    memset(marker->txt,0,24);
    marker->len = 0;
    strcpy(marker->txt,str);
    marker->len = strlen(str);
}


bool marker_found(char *marker) {
    int pos = 0;  
    for(int i = 0 ; i < str_idx; ++i){
    char c= str[i];

    if (c == -1) return false;  
    if (c == marker[pos]) {
        pos++;
       // putchar(pos+'0');                     
        if (marker[pos] == '\0') {  
            pos = 0;                
            return true;           

        }
    } else if (c == marker[0]) {
        pos = 1;       
    } else {
        pos = 0;        
    }
    }
    return false;
}


u32 decompose_addr(int marker_len) {

    char c = '\0';

    u32 addr = 0;
    str_idx-=(1+marker_len);


    for(int i = 0; i < str_idx; ++i){
        c = str[i];
       // printf("[%c %x]\n",c,c);

        addr+=  (c - (c > 0x39 ? 0x37 : 0x30))* pow(16,str_idx-(i+1));
    }

    return addr;
}


u8 test_val = 0; // for writing random to chip
u32 write_addr = 0;
bool readMode = false;


void console() {
    str_idx =0;

    while(1){

        int in = getchar();

        if(readMode){
            if(in>EOF){
                str[buffer_bytes++]  = (u8)in;
                
            }
            if(buffer_bytes >= PAGE_SIZE)
                {
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                    sleep_ms(200);
                   // write_page(write_addr,str,PAGE_SIZE,false,0x0); // buffer write
                    /*for(int i = 0 ; i< PAGE_SIZE; ++i) {
                        printf("%02x",str[i]);
                    }*/
                    readMode = false;
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                    str_idx = 0;
                    puts("wc\n");


                }
            else
                continue;
        }



        if(in>EOF){
            str[str_idx++]  = (u8)in;

            if(marker_found("$msgEnd")){
                str_idx-=6;

                 if(marker_found("$pageIn")) {
                    readMode = true;
                    buffer_bytes =0;
                    str_idx = 0;
                    continue;
                 }

                 /*if(marker_found("$pageWrite")) {

                    //str_idx-=11; // remove 0xd

                    //if(buffer_bytes==PAGE_SIZE){
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

                        //write_page(write_addr,str,PAGE_SIZE,false,0x0); // buffer write
                   // }
                }*/

                else if(marker_found("$pageWAddr")) {
                    write_addr = decompose_addr(10)&0x3ffff;
                   // printf("page address set to 0x%x\n",write_addr);

                }
                

                else if(marker_found("$pageRead"))
                    read_page((decompose_addr(9)&0x3ffff));   // page read

                else if(marker_found("$pageClr")){
                    str_idx = 0;
                    memset(str,0,sizeof(str));
                }
                
                
                else if(marker_found("$pageSetTest")){
                    test_val = (u8)(decompose_addr(12)&0xff); // test pattern set
                    printf("set test pattern to \"%x.\"\n",test_val);
                }

                else if(marker_found("$pageWriteTest"))
                    write_page((decompose_addr(14)&0x3ffff),NULL,PAGE_SIZE,true,test_val); // test pattern write

                else if(marker_found("$blockSHA"))
                    get_block_SHA(decompose_addr(9)&0xfff);

                 else if(marker_found("$pageSHA"))
                    get_page_SHA(decompose_addr(8)&0x3ffff);

                else if(marker_found("$blockErase"))
                    erase_block((decompose_addr(15)&0xfff)); // block erase (takes row address only)       
                    
                else if(marker_found("$NAND__fullErase"))
                    erase_chip(); // full chip erase

                else if(marker_found("$NAND_id")) // id read
                    read_id();
                
                str_idx = 0;

                }

            } 
            
            else if(in == 0x8 && str_idx>0)
                str_idx -=1;

        }

    }

    

int main()
{

    stdio_init_all();

    pageBuffer = &str[0];

    
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    init_gpio();

    while (!stdio_usb_connected()) {
        sleep_ms(10);
    }
        sleep_ms(100);

   // read_page((12 &0x3ffff)<<12);

    console();

  // for(int i = 0; i < PAGE_COUNT * BLOCK_COUNT; ++ i)
   //     read_page();

}
