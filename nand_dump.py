import serial
import time
import hashlib
import pathlib
import console_commands as cc

addr = 0
sector = 0
start_time = time.time()

BLOCKS = 8
PAGES = 64
PAGE_SIZE = 2112

BLOCK_SIZE = (2112 * 64) # 0x21000


BYTES_START = "$bs\r\n".encode()
FILENAME = F'nand_dumps/nand-{time.time()}.bin'
ser = serial.Serial("COM13", 115200,timeout=3,write_timeout=3)

pathlib.Path("nand_dumps").mkdir(parents=True, exist_ok=True)
f = open(FILENAME, 'wb')


# compute per-block hash
do_block_SHA = True



last_time = time.time()
addr = 0
for i in range(0,BLOCKS*PAGES):


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
    log = open('nand_dumps/log-'+str(int(time.time()))+".txt", 'w')
    f = open(FILENAME, 'rb') 

    print("=====SHA VERIFICATION (block)=====\n")    
    log.write("=====SHA VERIFICATION (block)=====\n")    
    
    addr = 0
    offset = 0 
    for i in range(0,BLOCKS):
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



