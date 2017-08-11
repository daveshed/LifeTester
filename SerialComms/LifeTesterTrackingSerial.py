import serial
import time
# import io
# import csv

print('Python script to log data from LifeTester via serial port.\n\
Hit ctrl + c to interrupt.')
port = input('Select COM port...(eg. COM3)\n')
ser = serial.Serial(port, 9600, timeout=0)
ser.reset_input_buffer()
ser.reset_output_buffer()
time.sleep(1)

serline = ''
mylist = []

try:
	while ser.isOpen():
		if ser.inWaiting():  # Or: while ser.inWaiting():
			serchar = ser.read().decode()

			if serchar == '\n':
				print(serline)
				mylist.append(serline.split(','))
				serline = ''
			else :
				if serchar != '\r':
					serline += serchar

except KeyboardInterrupt:
    print('interrupted!')

ser.close()
ser.__del__()

print(mylist)

# how to print out the same element from lists within a list...
# [[1,2,3],[4,5,6],[7,8,9]]
# [a[i][0] for i in range((a))]
# NB a[0] = [1,2,3]