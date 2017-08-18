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
data = []

plt.ion()

try:
	while ser.isOpen():
		if ser.inWaiting():  # Or: while ser.inWaiting():
			serchar = ser.read().decode()

			if serchar == '\n':
				print(serline)
				data.append(serline.split(','))
				serline = ''

				plt.figure(1)
				plt.title('IV scan')
				plt.ylabel('ADC code (current)')
				plt.xlabel('DAC code (voltage)')
				plt.plot(
					[x[3] for x in data if x[0] == 'scan' and x[2] == 'a'],
					[x[4] for x in data if x[0] == 'scan' and x[2] == 'a'],
					'r',
					[x[3] for x in data if x[0] == 'scan' and x[2] == 'b'],
					[x[4] for x in data if x[0] == 'scan' and x[2] == 'b'],
					'b'
						)

				plt.figure(2)
				plt.subplot(211)
				plt.title('Tracking MPP')
				plt.xlabel('time')
				plt.ylabel('DAC code (voltage)')
				plt.plot(
					[x[1] for x in data if x[0] == 'track' and x[2] == 'a'],
					[x[3] for x in data if x[0] == 'track' and x[2] == 'a'],
					'r',
					[x[1] for x in data if x[0] == 'track' and x[2] == 'b'],
					[x[3] for x in data if x[0] == 'track' and x[2] == 'b'],
					'b'
						)

				plt.subplot(212)
				plt.xlabel('time')
				plt.ylabel('ADC code (current)')
				plt.plot(
					[x[1] for x in data if x[0] == 'track' and x[2] == 'a'],
					[x[4] for x in data if x[0] == 'track' and x[2] == 'a'],
					'r',
					[x[1] for x in data if x[0] == 'track' and x[2] == 'b'],
					[x[4] for x in data if x[0] == 'track' and x[2] == 'b'],
					'b'
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