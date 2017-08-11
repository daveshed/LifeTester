import serial
import time
import matplotlib.pyplot as plt

print('Python script to log data from LifeTester via serial port.\n\
Hit ctrl + c to interrupt.')
port = input('Select COM port...(eg. COM3)\n')
ser = serial.Serial(port, 9600, timeout=0)
ser.reset_input_buffer()
ser.reset_output_buffer()
time.sleep(1)

serline = ''
mylist = []

plt.ion()

try:
	while ser.isOpen():
		if ser.inWaiting():  # Or: while ser.inWaiting():
			serchar = ser.read().decode()

			if serchar == '\n':
				print(serline)
				mylist.append(serline.split(','))
				serline = ''
				plt.plot(
					[mylist[i][3] for i in range(len(mylist)) if mylist[i][0] == 'scan' and mylist[i][2] == 'a'],
					[mylist[i][4] for i in range(len(mylist)) if mylist[i][0] == 'scan' and mylist[i][2] == 'a']
						)
				plt.pause(0.001)
				plt.clf()
				plt.draw()
			else :
				if serchar != '\r': #ignore \r char
					serline += serchar

except KeyboardInterrupt:
	print('interrupted!')

ser.close()
ser.__del__()


# how to print out the same element from lists within a list...
# [[1,2,3],[4,5,6],[7,8,9]]
# [a[i][0] for i in range(len(a))]
# NB a[0] = [1,2,3]