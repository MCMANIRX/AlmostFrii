import serial
import time
import hashlib
import pathlib
import zlib
PAGE_SIZE = 2112
PAGE_COUNT = 64
BLOCK_SIZE = 2112 * 64
CHUNK_SIZE = 264
EOF = "$msgEnd"

BYTES_START = "$bs".encode()
BYTES_FINISHED = "$bf".encode()


sector = 0
start_time = time.time()


pathlib.Path("log").mkdir(parents=True, exist_ok=True)
log = open('log/log-'+str(int(time.time()))+".txt", 'w')


f = open('fwd.bin', 'rb') # input NAND image


ser = serial.Serial("COM13", 115200,timeout=3, write_timeout= 1, xonxoff=False, rtscts=False, dsrdtr=False)

time.sleep(0.5)
last_time = time.time()


addr = 0x0
offset = 0x0

BLOCKS = 8
RANGE = 0x40 * BLOCKS

do_write = False
do_test_write = False
do_page_SHA = False
do_block_SHA = True


if do_write:
    log.write("=====FILE WRITE=====\n")
    for i in range(0,RANGE):


        if(addr%(0x40)==0):

            update = (F"writing block {sector} ({addr*2112} bytes, {(addr*2112) / 1E6}MB), time = {time.time()-last_time} seconds...")
            print(update)
            log.write(update+"\n")

            last_time = time.time()
            sector+=1

            
        # set address
        payload = ('&{:X}'.format(addr)+"$pageWAddr"+EOF).encode()
        ser.write(payload)

        sha_good = False
        time_out = time.time()
        while(True):
            # if local SHA and SHA of buffer on pico are equal, permit write to NAND
            if sha_good:
                ser.write('w'.encode())     
                break
                


            if time.time() > 2 +time_out:
                print(F"[{(time.time())}] ERROR: no good SHA")
                log.write(F"[{(time.time())}] ERROR: no good SHA")
                quit()
                
            # force pico to enter data input/read mode 
            payload = ("$pageIn"+EOF).encode()
            ser.write(payload)

            f.seek(offset*(PAGE_SIZE))
            byte_buffer = f.read(PAGE_SIZE)
            original_sha256 = hashlib.sha256(byte_buffer).hexdigest()
            

            # send and write page data 2112/8 = 264 bytes at a time
            ser.write(BYTES_START)
            for j in range(0,8):
                bo = CHUNK_SIZE*j
                payload = byte_buffer[bo : bo + CHUNK_SIZE] 
            #    per page CRC32
            #    crc = zlib.crc32(payload) & 0xffffffff
            #    ser.write(crc.to_bytes(4,"little"))
                ser.write(payload)
            ser.write(BYTES_FINISHED)

                    
            time_out1 = time.time()
            while(True):
                if time.time() > 0.1 +time_out1:
                    break

                line = ser.readline()[:-2].decode('utf-8')
                if("SHA: " in line):
                    sha256 = line.split("SHA: ", 1)[1]
                    if(sha256 == original_sha256):
                        sha_good = True   
                        #print(sha256)               
                    else:
                        print("bad SHA. Retrying!")
                        ser.write('c'.encode())     

                    break
                

                
        # wait for write complete ack
        time_out = time.time()
        while True:
            line = ser.readline().decode('utf-8')
            
            if time.time() > 1 +time_out:
                print(F"[{(time.time())}] ERROR: timeout on write ACK")
                log.write(F"[{(time.time())}] ERROR: timeout on write ACK, page {addr}, block {int(addr/0x40)}")
                
            elif 'WERR' in line:
                print(F"[{(time.time())}] ERROR: line write error")
                log.write(F"[{(time.time())}] ERROR: line write error\n")
                quit()
                
            elif 'wc' in line:
                break
        
        addr+=1
        offset+=1

    print("Completed writing.")
    log.write("Completed writing.\n")

    end_time = time.time()

    elapsed_time = end_time - start_time

    print(f"Start Time: {start_time}")
    print(f"End Time: {end_time}")
    print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")

    log.write(f"Start Time: {start_time}\n")
    log.write(f"End Time: {end_time}\n")
    log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")

    time.sleep(0.01)
    ser.write("$NAND_ID$msgEnd".encode()) # buffer to be safe
    time.sleep(0.1)
    
if do_test_write:
    payload = ('&{:X}'.format(0xAD)+"$pageSetTest"+EOF).encode()
    ser.write(payload)
    time.sleep((0.01))
    for i in range(0,RANGE):
        payload = ('&{:X}'.format(i)+"$pageWriteTest"+EOF).encode()
        ser.write(payload)

        time.sleep((0.01))
        if i%64 == 0:
            print(F"block {int(i/64)}")


if do_page_SHA:
    log.write("=====SHA VERIFICATION (page)=====\n")    
    f.seek(0x0)
    addr = 0
    offset = 0 
    for i in range(0,BLOCKS*PAGE_COUNT):
        f.seek(offset*(PAGE_SIZE))
        print(hex(offset*PAGE_SIZE))
        original_sha256 = hashlib.sha256(f.read(PAGE_SIZE)).hexdigest()
        
        payload = ('&{:X}'.format(addr)+"$pageSHA"+EOF).encode()
        ser.write(payload)
        
    
        time_out = time.time()
        while(True):
            line = ser.readline()[:-2].decode('utf-8')
            if("SHA: " in line):
                sha256 = line.split("SHA: ", 1)[1]
                if(not sha256 or (sha256 != original_sha256)):
                    print(F"SHA Mismatch: page {addr}\norig: {original_sha256}\nchip: {sha256}")
                    log.write(F"SHA Mismatch: page {addr}\noriginal: {original_sha256}\nchip: {sha256}\n")

            # print(F"orig: {original_sha256}\nchip: {sha256}")
                break
            
            if time.time() > 1 +time_out:
                print("ERROR: timeout on SHA256")
                quit()
    
        
        
        offset+=1
        addr+=1
        
    print("Completed page verification.")
    log.write("Completed page verification.\n")#ser.write(clear.encode())

if do_block_SHA:
    log.write("=====SHA VERIFICATION (block)=====\n")    
    addr = 0
    offset = 0 
    for i in range(0,BLOCKS):
        f.seek(offset*(BLOCK_SIZE))
        original_sha256 = hashlib.sha256(f.read(BLOCK_SIZE)).hexdigest()
        
        payload = ('&{:X}'.format(addr)+"$blockSHA"+EOF).encode()
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
ser.close()

end_time = time.time()

elapsed_time = end_time - start_time

log.write(f"Start Time: {start_time}\n")
log.write(f"End Time: {end_time}\n")
log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")
