# script to calculate test data for an ideal diode and report max power point.

from math import exp
from collections import namedtuple

# assume ideal diode. ignore shunt and series resistance
I_L = 1.0
I_0 = 1.0E-09
V_T = 0.0259
N = 1.0
GAIN = 1.0
V_REF = 2.048
DAC_RESOLUTION = 8
HEADERS = ('code', 'voltage', 'current', 'power')

def code_to_volts(x):
    volts = (x / float(2**DAC_RESOLUTION)) * GAIN * V_REF
    return volts


def shockley_get_current(V):
    I = I_L - I_0 * (exp(V / (N * V_T)) - 1)
    if I > 0.0:
        return I
    else:
        return 0.0

def calculate_diode_data(codes):
    # calculate the diode data
    iv = namedtuple('iv', HEADERS)
    mpp = iv(0, 0.0, 0.0, 0.0)
    data = []
    for x in range(codes):
        volts = code_to_volts(x)
        current = shockley_get_current(volts)
        power = current * volts
        new_iv = iv(x, volts, current, power)

        data.append(new_iv)
        # updating the mpp if needed
        if power > mpp.power:
            mpp = new_iv

    return data, mpp



diode_data, mpp = calculate_diode_data(2**DAC_RESOLUTION)

# print out the data
for iv_point in diode_data:
    print '{{{0:3f}, {1:0.6e}, {2:0.6e}}},'\
    .format(iv_point.voltage, 
        iv_point.current,
        iv_point.power)

print 'code = {}, v_mpp = {}'.format(mpp.code, mpp.voltage)