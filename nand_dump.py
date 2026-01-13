import serial
import time
import hashlib
import pathlib
import console_commands as cc
import sys


PAGE_COUNT = 64
PAGE_SIZE = 2112
BLOCK_COUNT = 4096

BLOCK_SIZE = (2112 * 64) # 0x21000


BYTES_START = "$bs\r\n".encode()
FILENAME = F'nand_dumps/nand-{time.time()}.bin'

start_time = time.time()

nblocks = BLOCK_COUNT
block_addr = 0
serial_port = ""
debug = False

if len(sys.argv)>1:
    for q in range (1, len(sys.argv)):
        arg = sys.argv[q]
        
        if '-' in arg:
            option = arg.split('-')[1]


            if 'P' in option:
                option = arg.split("P")
                serial_port = option[1]
                continue
            
            if 'l' in option:
                option = arg.split("l")
                try:
                    nblocks = int(option[1])
                except ValueError:
                    print("ERROR: invalid block number")
                    quit()
                continue
            
            if 'a' in option:
                option = arg.split("a")
                try:
                    block_addr = int(option[1],16)
                except ValueError:
                    print("ERROR: invalid address")
                    quit()
                continue
            
            if 'd' in option:
                debug = True

if not serial_port:
    print(f"ERROR: No serial port given")
    quit()


opt_str = F"Reading {nblocks} blocks from start address {block_addr} with port {serial_port}."
    
print(opt_str) 
if debug:
    quit()
    


ser = serial.Serial(serial_port, 115200,timeout=3, write_timeout= 3)


pathlib.Path("nand_dumps").mkdir(parents=True, exist_ok=True)
f = open(FILENAME, 'wb')


# compute per-block hash
do_block_SHA = True

# setup values 
addr = block_addr *PAGE_COUNT
offset = addr
end_offset = (nblocks *PAGE_COUNT) + addr
sector = block_addr

last_time = time.time()
while addr < end_offset:


    if(addr%(64)==0):

        print("reading block",sector,"(",addr*2112,"bytes,",(addr*2112 / 1E6),"MB), time =",time.time()-last_time,"seconds...")
        last_time = time.time()
        sector+=1


    payload = '&{:X}'.format(addr)
    payload += cc.READ_PAGE + cc.MESSAGE_END +'\r'
    
    byte_count = 0
    #time.sleep(0.1)
    start = time.time()
    ser.write(payload.encode())
    msgNum = 0
    while byte_count < PAGE_SIZE and time.time() - start <= 10:
        buffer = None
        
        if(ser.readline() == BYTES_START):
            buffer = ser.read(PAGE_SIZE)
            byte_count+=PAGE_SIZE
            f.write(buffer)
            
    if time.time() - start >= 10:
        print("Timeout error. Exiting!")
        quit()
    addr+=1
    
    
print("done")

end_time = time.time()

elapsed_time = end_time - start_time

print(f"Start Time: {start_time}")
print(f"End Time: {end_time}")
print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")


if do_block_SHA:
    start = time.time()
    log = open(F'{FILENAME}_log.txt', 'w')
    f = open(FILENAME, 'rb') 
    print("=====SHA VERIFICATION (block)=====\n")    
    log.write("=====SHA VERIFICATION (block)=====\n")    
    
    
    addr = block_addr
    offset = 0
    end_offset = nblocks + addr
    
    while addr < end_offset:
        f.seek(offset*(BLOCK_SIZE))
        original_sha256 = hashlib.sha256(f.read(BLOCK_SIZE)).hexdigest()
        
        payload = ('&{:X}'.format(addr)+cc.BLOCK_SHA+cc.MESSAGE_END).encode()
        ser.write(payload)
        
        
        time_out = time.time()
        while(True):
            line = ser.readline()[:-2].decode('utf-8')
            if("SHA: " in line):
                sha256 = line.split("SHA: ", 1)[1]
                if(sha256 == original_sha256):
                    print(F"SHA match for block {addr}\norig: {original_sha256}\nchip: {sha256}")
                    log.write(F"SHA match for block {addr}\norig: {original_sha256}\nchip: {sha256}\n")

                else:
                    print(F"SHA Mismatch: block {addr}\noriginal: {original_sha256}\nchip: {sha256}")
                    log.write(F"SHA Mismatch: block {addr}\noriginal: {original_sha256}\nchip: {sha256}\n")

                #print(F"block {addr} SHA: {sha256}")
                break
            if time.time() > 1 +time_out:
                print("ERROR: timeout on SHA256")
                log.write("ERROR: timeout on SHA256\n")
                
                quit()
        
        
        
        offset+=1
        addr+=1

        

        

    print("Completed block verification.")
    log.write("Completed block verification.\n")#ser.write(clear.encode())
    
    end_time = time.time()

    elapsed_time = end_time - start_time

    log.write(f"Start Time: {start_time}\n")
    log.write(f"End Time: {end_time}\n")
    log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")
    print(f"Start Time: {start_time}\n")
    print(f"End Time: {end_time}\n")
    print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")
    
ser.close()



