#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include <math.h>

#include "commands.h"

#define BLOCK_COUNT 4096
#define PAGE_COUNT 64
#define PAGE_SIZE 2112


u8 str[100] = {0};
u8 str_idx = 0;


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


u32 decompose_addr() {

    char c = '\0';

    u32 addr = 0;
    str_idx-=2;

    for(int i = 0; i < str_idx; ++i){
        c = str[i];
        addr+=  (c - (c > 0x39 ? 0x37 : 0x30))* pow(16,str_idx-(i+1));
    }

    return addr;
}






void console() {
    str_idx =0;

    while(1){

        u8 in = getchar();

        if(in){
            str[str_idx++]  = in;

            if(in == 0xd){

                if(marker_found("p"))
                    read_page((decompose_addr()&0x3ffff));   // page read

                else if(marker_found("e"))
                    erase_block((decompose_addr()&0x3ffff)); // block erase (takes row address only)

                else if(marker_found("t"))
                    write_page((decompose_addr()&0x3ffff),NULL,PAGE_SIZE,true); // test pattern write

                else if(marker_found("b"))
                    write_page((decompose_addr()&0x3ffff),NULL,PAGE_SIZE,false); // buffer write

                else if(marker_found("id")) // id read
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

    /*
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);*/
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
