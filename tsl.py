import gpiod
LED_PIN = 14
SENSOR_PIN = 3
led_time = 0
sensor_time = 0
led_end = 0
import socket, struct, sys, time, threading
import time
import busio
import digitalio
import board
import adafruit_tsl2561
def led_start(i):
 global led_time
 global sensor_time
 global led_end
 global sensor_time
 while sensor_time == 0:
     time.sleep(0.1)
 try:
        time_nanosec = time.time_ns() 
        led_time = time_nanosec
        led_line.set_value(1)
        print("Time in nanoseconds since the epoch led blink:", time_nanosec)
        time.sleep(1)
        led_line.set_value(0)
        time.sleep(1)
 finally:
    led_line.release()
    led_end = 1
    #led_time = 1


def sensor_start(change):
 global sensor_time
 global led_end
 i2c = busio.I2C(board.SCL, board.SDA)

# Create the TSL2561 instance, passing in the I2C bus
 tsl = adafruit_tsl2561.TSL2561(i2c)

# Print chip info
#print("Chip ID = {}".format(tsl.chip_id))
#print("Enabled = {}".format(tsl.enabled))
#print("Gain = {}".format(tsl.gain))
#print("Integration time = {}".format(tsl.integration_time))

 print("Configuring TSL2561...")

# Enable the light sensor
 tsl.enabled = True
 time.sleep(1)

# Set gain 0=1x, 1=16x
 tsl.gain = 0

# Set integration time (0=13.7ms, 1=101ms, 2=402ms, or 3=manual)
 tsl.integration_time = 0
 i = 0
 sensor_time = 1
 lux_prev = 1
 first_time = 0
 print("Getting readings...")
 while led_end == 0:
# Get raw (luminosity) readings individually
   broadband = tsl.broadband
   infrared = tsl.infrared

# Get raw (luminosity) readings using tuple unpacking
# broadband, infrared = tsl.luminosity

# Get computed lux value (tsl.lux can return None or a float)
   lux = tsl.lux
   if lux is not None and first_time == 0:
    if lux > 2 * lux_prev:
     sensor_time = time.time_ns()
     first_time = 1
     print(sensor_time) 

# Print results
#print("Enabled = {}".format(tsl.enabled))
#print("Gain = {}".format(tsl.gain))
#print("Integration time = {}".format(tsl.integration_time))
#print("Broadband = {}".format(broadband))
#print("Infrared = {}".format(infrared))
   #if lux is not None:
     #print(format(lux))
     #print("\n")
    #print(sensor_time)
    


# Disble the light sensor (to save power)
 tsl.enabled = False


if __name__ == '__main__':
    # Set input and output pins
    j = 0
    while j < 10 :
     chip = gpiod.Chip('gpiochip4')
     led_line = chip.get_line(LED_PIN)
     led_line.request(consumer="LED", type=gpiod.LINE_REQ_DIR_OUT)
     i = 0
     tblink = threading.Thread(target=led_start, args=(i,))
     tsense = threading.Thread(target=sensor_start, args=(i,))
    
     tblink.start()
     tsense.start()

     tblink.join()
     tsense.join()
     delay = sensor_time - led_time
     print(delay)
     sensor_time = 0
     led_time = 0
     led_end = 0
     j = j + 1
   
