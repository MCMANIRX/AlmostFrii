import serial
import time
import hashlib
import pathlib
import zlib
import console_commands as cc
import sys

PAGE_SIZE = 2112
PAGE_COUNT = 64
BLOCK_SIZE = (2112 * 64)
CHUNK_SIZE = 264

BYTES_START = cc.BYTES_START.encode()
BYTES_FINISHED = cc.BYTES_FINISHED.encode()
PAGE_IN = (cc.PAGE_IN+cc.MESSAGE_END).encode()

EMPTY_BLOCK = "49a871401dfd0c0897d7beb7956fde1c59eb86c446f627e1dda9c6e58be67118" # all FF's


sector = 0



#f = open('fwd.bin', 'rb') # input NAND image



time.sleep(0.5)
last_time = time.time()

block_addr = 0x0
addr = 0x0
offset = 0x0

nblocks = 0

do_write = False
do_test_write = False
do_page_SHA = False
do_block_SHA = True
debug = False

f  = []
nand_file = ""
serial_port = ""

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


            
            if 'w' in option and not do_test_write:
                do_write = True
            if 't' in option and not do_write:
                do_test_write = True
            if 'p' in option:
                do_page_SHA = True
            if 'b' in option:
                do_block_SHA = True
            if 'B' in option:
                do_block_SHA = False
            if 'd' in option:
                debug = True

                
        else:
            nand_file = arg
            try:
                f = open(nand_file,"rb")
            except FileNotFoundError:
                print("ERROR: File does not exist")
                quit()
            except PermissionError:
                print("ERROR: No permission to read the file")
                quit()
            except OSError as e:
                print(f"ERROR: File read error: {e}")
                quit()
    
if not nand_file:
    print("ERROR: No NAND file")
    quit()

if not serial_port:
    print(f"ERROR: No serial port given")
    quit()

opt_str = "\nDoing "
if do_test_write:
    opt_str+="test write "
elif do_write:
    opt_str+="write "
if do_write or do_test_write:
    opt_str+=F"from image {nand_file}"
    if (do_block_SHA or do_page_SHA):
        opt_str+=" with "

if do_page_SHA:
    opt_str+="per-page "
if do_block_SHA:
    if do_page_SHA:
        opt_str+="and "
    opt_str+="per-block "
if do_block_SHA or do_page_SHA:
    opt_str+="SHA256 calculation"

opt_str+=F", starting at block address {hex(addr)} for {nblocks} blocks"
opt_str+=F" via port {serial_port}"
    
print(opt_str) 
if debug:
    quit()

pathlib.Path("log").mkdir(parents=True, exist_ok=True)
log = open('log/log-'+str(int(time.time()))+".txt", 'w')
log.write(opt_str)

ser = serial.Serial(serial_port, 115200,timeout=3, write_timeout= 1, xonxoff=False, rtscts=False, dsrdtr=False)





time.sleep(0.5)


# setup values 
addr = block_addr *PAGE_COUNT
offset = addr
end_offset = (nblocks *PAGE_COUNT) + addr
sector = int(addr/PAGE_COUNT)
f.seek(offset)

start_time = time.time()

if do_write and not do_test_write:
    log.write("=====FILE WRITE=====\n")
    while addr < end_offset:


        if(addr%(0x40)==0):
            sha256 = hashlib.sha256(f.read(BLOCK_SIZE)).hexdigest()
            #print(sha256)
            if sha256 == EMPTY_BLOCK:
                print(F"skipping null block {sector}")
                sector+=1
                addr+=PAGE_COUNT
                offset+=PAGE_COUNT
                continue

            update = (F"writing block {sector} ({addr*2112} bytes, {(addr*2112) / 1E6}MB), time = {time.time()-last_time} seconds...")
            print(update)
            log.write(update+"\n")

            last_time = time.time()
            sector+=1
            

            
        # set address
        payload = ('&{:X}'.format(addr)+cc.SET_W_ADDR+cc.MESSAGE_END).encode()
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
            ser.write(PAGE_IN)

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
    ser.write((cc.NAND_ID+cc.MESSAGE_END).encode()) # buffer to be safe
    time.sleep(0.1)
    
if do_test_write and not do_write:
    payload = ('&{:X}'.format(0xAD)+cc.SET_W_TST_VAL+cc.MESSAGE_END).encode()
    ser.write(payload)
    time.sleep((0.01))
    for i in range(0,end_offset):
        payload = ('&{:X}'.format(i)+cc.TEST_W+cc.MESSAGE_END).encode()
        ser.write(payload)

        time.sleep((0.01))
        if i%64 == 0:
            print(F"block {int(i/64)}")


if do_page_SHA:
    start_time = time.time()
    log.write("=====SHA VERIFICATION (page)=====\n")    
    addr = block_addr *PAGE_COUNT
    offset = addr
    end_offset = (nblocks *PAGE_COUNT) + addr
    while addr < end_offset:
        f.seek(offset*(PAGE_SIZE))
        #print(hex(offset*PAGE_SIZE))
        original_sha256 = hashlib.sha256(f.read(PAGE_SIZE)).hexdigest()
        
        payload = ('&{:X}'.format(addr)+cc.PAGE_SHA+cc.MESSAGE_END).encode()
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
        
    print("\nCompleted page verification.")
    log.write("\nCompleted page verification.\n")#ser.write(clear.encode())
    
    end_time = time.time()
    elapsed_time = end_time - start_time
    log.write(f"Start Time: {start_time}\n")
    log.write(f"End Time: {end_time}\n")
    log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")
    
    print(f"Start Time: {start_time}")
    print(f"End Time: {end_time}")
    print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")

if do_block_SHA:
    start_time = time.time()
    log.write("=====SHA VERIFICATION (block)=====\n")    
    addr = block_addr
    offset = addr
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

        

    print("\nCompleted block verification.")
    log.write("\nCompleted block verification.\n")#ser.write(clear.encode())
    end_time = time.time()

    elapsed_time = end_time - start_time

    log.write(f"Start Time: {start_time}\n")
    log.write(f"End Time: {end_time}\n")
    log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")
    
    print(f"Start Time: {start_time}")
    print(f"End Time: {end_time}")
    print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")
    
ser.close()
f.close()


