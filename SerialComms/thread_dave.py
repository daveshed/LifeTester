from Queue import Queue
from threading import Thread
import matplotlib.pyplot as plt
import time
import math
import random

# number of queues
num_q = 5
# list of queues
data_queues = [Queue() for _ in range(num_q)]

def get_data(q):
    """Thread function that posts data to queue q."""
    i = 0
    angle_res = 200.0
    sampling_freq = 2.0;
    phase = random.random() * math.pi
    while True:
        data = math.sin(phase + 2 * math.pi * i / angle_res)
        q.put((i / sampling_freq, data))
        i += 1

# Launch a load of sensor threads, which are all unique
sensors = [Thread(target=get_data, args=(q,)) for q in data_queues]

# launch the threads
for sensor in sensors:
    # daemon means it's a background process, so it'll end with
    # the main one
    sensor.setDaemon(True)
    sensor.start()

# lists for each data stream
xdata = [[] for _ in range(num_q)]
ydata = [[] for _ in range(num_q)]
 
# initialize plot stuff
plt.show()
axes = plt.gca()
axes.set_title("Dave's Data")
axes.set_ylim(-2,2)
axes.set_xlim(0, 100)
lines = [axes.plot(xdata[i], ydata[i], label="Thread {}".format(i))[0] \
         for i in range(num_q)]
plt.legend()

# grab data & plot it forever
# you get this depreciation warning ~ cba to fix...
while True:
   for j, q in enumerate(data_queues):
       (t, data) = q.get()
       if t > 100:
           axes.set_xlim(0, t)
       q.task_done()
       xdata[j].append(t)
       ydata[j].append(data)
       lines[j].set_xdata(xdata[j])
       lines[j].set_ydata(ydata[j])
   plt.draw()
   plt.pause(1e-17)
   time.sleep(0.01)

