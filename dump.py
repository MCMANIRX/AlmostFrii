import serial
import time

addr = 0
sector = 0
start_time = time.time()

ser = serial.Serial("COM13", 115200,timeout=3)

f = open('nand.bin', 'wb')

clear = "clr\r\n"
ser.write(clear.encode())
time.sleep(0.5)
last_time = time.time()
addr = 0x0
for i in range(0,64 * 4096):


    if(addr%(64)==0):

        sector+=1
        print("reading block",sector,"(",addr*2112,"bytes,",(addr*2112 / 1E6),"MB), time =",time.time()-last_time,"seconds...")
        last_time = time.time()


    payload = '{:X}'.format(addr)
    payload+='p' 
    payload+='\r\n'   
    ser.write(payload.encode())
    
    byte_count = 0
    start = time.time()
    while byte_count < 2112 and time.time() - start <= 10:
        buffer = ser.readline()[:-2].decode('utf-8')
        if not buffer:
            print("ERROR!")
            break
        
        #print(buffer)
        if 'ERR' in buffer:
            print("ERROR!",addr)
            print(buffer)
            #print("Error at ",hex(payload),"! maybe I should start day trading...")
            break  
        #print(hex(int(buffer,16)),bin(addr),hex(addr),addr)
        f.write(int(buffer,16).to_bytes(1,"big"))
        byte_count+=1
    if time.time() - start >= 10:
        print("Timeout error. Exiting!")
        quit()
    addr+=1
    
    
print("done")
#ser.write(clear.encode())
#ser.close()

end_time = time.time()

elapsed_time = end_time - start_time

print(f"Start Time: {start_time}")
print(f"End Time: {end_time}")
print(f"Elapsed Time: {elapsed_time:.2f} seconds, {elapsed_time/3600.0:.2f} hours.")