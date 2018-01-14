# LifeTester
A solar cell maximum power point (MPP) tracking system based on Arduino. See www.theonlineshed.com for circuit schematics and more discussion. Briefly, a micro-controller (ATMEGA328) is interfaced with an analog-to-digital converter (ADC) and digital-to-analog converted (DAC) via serial peripheral interface (SPI): a voltage is applied to the device under test (DUT) from the DAC and current is measured from a basic current sense circuit consisting of sense resistor and inverting op-amp whose output is fed into the ADC input. Two channels (A and B) are available in hardware at present corresponding to sub-cells of a single device.

## Algorithm
The MPP is tracked by a simple hill climbing method:
1) Initialisation of peripherals following power cycle/reset signal
2) Initial voltage sweeps are carried out first to determine an initial guess for MPP (drive voltage)
3) DAC outputs are then set to initial MPP guess and tracking continues indefinitely
4) If an error is raised, that channel will remain in error state until the LifeTester is reset

## Interfacing
Data from the LifeTester is transmitted over as a byte string over I2C. Up to 112 LifeTesters could be connected in this fashion as slaves to a master device. Presently, a Raspberry Pi serves as a master (_see_ project daveshed/LifeTesterInterface).
