# SPDX-FileCopyrightText: 2021 ladyada for Adafruit Industries
# SPDX-License-Identifier: MIT
import time
import busio
import digitalio
import board
import adafruit_mcp3xxx.mcp3008 as MCP
from adafruit_mcp3xxx.analog_in import AnalogIn

# create the spi bus
spi = busio.SPI(board.SCLK_1, MOSI=board.MOSI_1, MISO=board.MISO_1) 

# create the cs (chip select)
#cs = digitalio.DigitalInOut(board.D5)
#spi = busio.SPI(clock=21, MISO=19, MOSI=20)

#create the chip select(s)
cs = digitalio.DigitalInOut(board.D16)
# create the mcp object
mcp = MCP.MCP3008(spi, cs)

# create an analog input channel on pin 0
chan = AnalogIn(mcp, MCP.P0)

t_end = time.time() + 1 
i = 0
while time.time() < t_end:
    time_nanosec2 = time.time_ns() 
    #print('Raw ADC Value: ', chan.value)
    chan.value
    #print('ADC Voltage: ' + str(chan.voltage) + 'V')
    time_nanosec = time.time_ns() 
    delay =  time_nanosec - time_nanosec2
    print(delay)
    i = i + 1
#print(i)

