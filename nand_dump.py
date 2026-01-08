import serial
import time
import pathlib

addr = 0
sector = 0
start_time = time.time()

BLOCKS = 8
PAGES = 64
PAGE_SIZE = 2112

BYTES_START = "$bs\r\n".encode()

ser = serial.Serial("COM13", 115200,timeout=3,write_timeout=3)

pathlib.Path("nand_dumps").mkdir(parents=True, exist_ok=True)
f = open(F'nand_dumps/nand-{time.time()}.bin', 'wb')

clear = "clr\r\n"
ser.write(clear.encode())
time.sleep(0.5)
last_time = time.time()
addr = 0
for i in range(0,BLOCKS*PAGES):


    if(addr%(64)==0):

        sector+=1
        print("reading block",sector,"(",addr*2112,"bytes,",(addr*2112 / 1E6),"MB), time =",time.time()-last_time,"seconds...")
        last_time = time.time()


    payload = '&{:X}'.format(addr)
    payload +='$pageRead$msgEnd\r' 
    
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
ser.close()

end_time = time.time()

elapsed_time = end_time - start_time

print(f"Start Time: {start_time}")
print(f"End Time: {end_time}")
print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")