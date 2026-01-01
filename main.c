#include <stdio.h>
#include "pico/stdlib.h"
#include "stdlib.h"
#include <math.h>
#include "crc.h"
#include <string.h>
#include "pico/cyw43_arch.h"
#include "tusb.h"


#include "commands.h"



#define BYTES_BEGIN "$beginStream$"
#define BYTES_END   "$endStream$"


u8 str[200] = {0};
u8 str_idx = 0;


typedef struct {
    char txt[24];
    int len;
} MARKER;


static void init_marker(MARKER * marker, char *_str) {
    memset(marker->txt,0,24);
    marker->len = 0;
    strcpy(marker->txt,_str);
    marker->len = strlen(_str);
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


u32 decompose_addr() {

    char c = '\0';

    u32 addr = 0;
    char *start = strchr(str,'&');
    char *end = strchr(str,'$');

    if((!(start && end)) || abs( ((int)(((u8*)start)-str))  -((int)(((u8*)end)-str)) ) <= 1) {

        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        while(1);
    }

    int start_idx = (int)((u8*)start-str);
    int end_idx = (int)((u8*)end-str);



    for (int i = start_idx + 1; i < end_idx; i++) {
        char c = str[i];

        u8 value;
        if (c >= '0' && c <= '9')
            value = c - '0';
        else if (c >= 'A' && c <= 'F')
            value = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            value = c - 'a' + 10;
        else
            break; // invalid hex

        addr = (addr << 4) | value;
    }

   // printf("addr: %x\n",addr);

    return addr;
}


u32 to_u32(char *buf, int len) {

    char c = '\0';
    u32 addr = 0;

    for (int i = 0; i < len; i++) {
        char c = str[i];

        u8 value;
        if (c >= '0' && c <= '9')
            value = c - '0';
        else if (c >= 'A' && c <= 'F')
            value = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')
            value = c - 'a' + 10;
        else
            break; // invalid hex

        addr = (addr << 4) | value;
    }


    return addr;
}


int read_byte_blocking(void) {
    uint8_t b;
    while (tud_cdc_read(&b, 1) != 1) {}
    return b;
}


int read_n_bytes(u8 * buf, size_t idx, size_t len) {
    int i = 0;

    while(i < len) {

        int in = read_byte_blocking();

        if(in!=EOF){
            buf[idx+i] = (u8)in;
            i++;
        }
    }

    return i;
}


u8 test_val = 0; // for writing random to chip
u32 write_addr = 0;
bool readMode = false;
bool writeComplete = false;
char failMsg[20] = {0};

void console() {
    str_idx =0;

    while(1){

        if(readMode){
            u8 chunk_idx = 0;
            writeComplete = false;


            // wait for chunk prefix
            u8 mrkr[3] = {0};
            read_n_bytes(mrkr,0,3);

            if(strncmp(mrkr,"$bs",3)!=0){
                strcpy(failMsg ,mrkr);
                goto end;
            }

            while(chunk_idx != 8) {

            /*

                // get CRC32
                u32 crc = 0;
                read_n_bytes((u8*)(&crc),0,4);

                // get chunk
                buffer_bytes += read_n_bytes(pageBuffer,chunk_idx*CHUNK_SIZE,CHUNK_SIZE);

                if(crc != crc32(&pageBuffer[chunk_idx*CHUNK_SIZE],CHUNK_SIZE)){

                    strcpy(failMsg ,"bad crc!");

                    goto end;
                }*/

                buffer_bytes += read_n_bytes(pageBuffer,chunk_idx*CHUNK_SIZE,CHUNK_SIZE);
                chunk_idx++;

            }

                // get chunk suffix
                read_n_bytes(mrkr,0,3);
                if(strncmp(mrkr,"$bf",3)!=0){
                    strcpy(failMsg ,"bad $bf!");
                    goto end;
                }
    

            if(buffer_bytes!=PAGE_SIZE){

                strcpy(failMsg ,"bad buffer bytes!");

                goto end;
            }
            
            // send SHA
            get_buffer_SHA();

            // wait for SHA good
            char ack = '\0';
            read_n_bytes(&ack,0,1);

            if(strncmp(&ack,"w",1)!=0){
                strcpy(failMsg,&ack);

                goto end;
            }

            
            write_page(write_addr,pageBuffer,PAGE_SIZE,false,0x0); // buffer write

            puts("wc\n"); // ack write complete
            
            writeComplete = true;

            end:

                readMode = false;
                str_idx = 0;
                buffer_bytes = 0;
                bool flicker = false;

                if(!writeComplete)
                    while(1) {
                        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, flicker);
                        flicker = !flicker;
                        sleep_ms(100);
                        printf("%s\n",failMsg);
                    }

                writeComplete = false;
                continue;
        }




        int in = read_byte_blocking();


        if(in>0){
            str[str_idx++]  = (u8)in;

            if(marker_found("$msgEnd")){
                str_idx-=6;



                 if(marker_found("$pageIn")) {
                    readMode = true;
                    buffer_bytes =0;
                    str_idx = 0;
                    memset(pageBuffer,0,PAGE_SIZE);

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
                    write_addr = decompose_addr()&0x3ffff;
                   // printf("page address set to 0x%x\n",write_addr);

                }
                

                else if(marker_found("$pageClr")){
                    str_idx = 0;
                    memset(str,0,sizeof(str));
                }
                
                
                else if(marker_found("$pageSetTest")){
                    test_val = (u8)(decompose_addr()&0xff); // test pattern set
                    printf("set test pattern to \"%x.\"\n",test_val);
                }

                else if(marker_found("$pageWriteTest")){
                    write_page((decompose_addr()&0x3ffff),NULL,PAGE_SIZE,true,test_val); // test pattern write
                }

                else if(marker_found("$pageRead")){
 
                    read_page((decompose_addr()&0x3ffff));   // page read
                }

                else if(marker_found("$blockSHA")){
                    get_block_SHA(decompose_addr()&0xfff);
                }
                 else if(marker_found("$pageSHA")){
                    get_page_SHA(decompose_addr()&0x3ffff);
                 }
                else if(marker_found("$blockErase")){
                    erase_block((decompose_addr()&0xfff)); // block erase (takes row address only)       
                }
                else if(marker_found("$NAND__fullErase")){
                    erase_chip(); // full chip erase
                }
                else if(marker_found("$NAND_id")) {// id read
                    read_id();
                }
                
                str_idx = 0;
                    memset(str,0,sizeof(str));

                }

            } 
            
           // else if(in == 0x8 && str_idx>0)
           //     str_idx -=1;

        }

    }

    

int main()
{

    stdio_init_all();


    //pageBuffer = &str[0];

    
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
