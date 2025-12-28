import serial
import time
import hashlib
import pathlib
PAGE_SIZE = 2112
BLOCK_SIZE = 2112 * 64
EOF = "$msgEnd"

sector = 0
start_time = time.time()



pathlib.Path("log").mkdir(parents=True, exist_ok=True)



f = open('fwd.bin', 'rb')
log = open('log/log-'+str(int(time.time()))+".txt", 'w')

#clear = "clr\r\n"
#ser.write(clear.encode())

ser = serial.Serial("COM13", 115200,timeout=3)

time.sleep(0.5)
last_time = time.time()



addr = 0x0
offset = 0x0

BLOCKS = 4096
RANGE = 0x40 * BLOCKS

log.write("=====FILE WRITE=====")
for i in range(0,RANGE):


    if(addr%(0x40)==0):

        update = (F"writing block {sector} ({addr*2112} bytes, {(addr*2112) / 1E6}MB), time = {time.time()-last_time} seconds...")
        print(update)
        log.write(update+"\n")

        last_time = time.time()
        sector+=1

        
    # set address
    payload = ('{:X}'.format(addr)+"$pageWAddr"+EOF).encode()
    ser.write(payload)

    
    # enter data read mode
    payload = ("$pageIn"+EOF).encode()
    ser.write(payload)

    # send and write page data 2112/8 = 264 bytes at a time
    f.seek(offset*PAGE_SIZE)
    for j in range(0,8):
        payload = f.read(264)
        ser.write(payload)
    #payload = ('$pageWrite'+EOF).encode()

   # ser.write(payload)
    time_out = time.time()
    while True:
        line = ser.readline().decode('utf-8')
        
        if time.time() > 1 +time_out:
            print(F"[{(time.time())}] ERROR: timeout on write ACK")
            log.write(F"[{(time.time())}] ERROR: timeout on write ACK")
            quit()
            
        elif 'WERR' in line:
            print(F"[{(time.time())}] ERROR: line write error")
            log.write(F"[{(time.time())}] ERROR: line write error\n")
            quit()
            
        elif 'wc' in line:
              break


    print(F"page {addr}")
        



    

    time.sleep(0.01)

    
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
ser.write("$NAND_ID$msgEnd".encode()) # clean it up
time.sleep(0.1)

'''
addr = 0
offset = 0 
for i in range(0,RANGE):
    f.seek(offset*(PAGE_SIZE))
    original_sha256 = hashlib.sha256(f.read(PAGE_SIZE)).hexdigest()
    
    payload = ('{:X}'.format(addr)+"$pageSHA"+EOF).encode()
    ser.write(payload)
    
    
    time_out = time.time()
    while(True):
        line = ser.readline()[:-2].decode('utf-8')
        if("SHA: " in line):
            sha256 = line.split("SHA: ", 1)[1]
            if(not sha256 or (sha256 != original_sha256)):
                print(F"SHA Mismatch: page {addr}\noriginal: {original_sha256}\nchip: {sha256}")
            print(F"original: {original_sha256}\nchip: {sha256}")
            break
        if time.time() > 1 +time_out:
            print("ERROR: timeout on SHA256")
            quit()
    
    
    
    offset+=1
    addr+=1'''
    
log.write("=====SHA VERIFICATION=====")    
addr = 0
offset = 0 
for i in range(0,BLOCKS):
    f.seek(offset*(BLOCK_SIZE))
    original_sha256 = hashlib.sha256(f.read(BLOCK_SIZE)).hexdigest()
    
    payload = ('{:X}'.format(addr)+"$blockSHA"+EOF).encode()
    ser.write(payload)
    
    
    time_out = time.time()
    while(True):
        line = ser.readline()[:-2].decode('utf-8')
        if("SHA: " in line):
            sha256 = line.split("SHA: ", 1)[1]
            if(not sha256 or (sha256 != original_sha256)):
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

    

print("Completed verification.")
log.write("Completed verification.\n")#ser.write(clear.encode())
ser.close()

end_time = time.time()

elapsed_time = end_time - start_time

log.write(f"Start Time: {start_time}\n")
log.write(f"End Time: {end_time}\n")
log.write(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.\n")