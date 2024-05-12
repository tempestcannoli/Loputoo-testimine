import gpiod
import time

SENSOR_PIN = 16
chip = gpiod.Chip('gpiochip4')
sensor_line = chip.get_line(SENSOR_PIN)
sensor_line.request(consumer="SENSOR", type=gpiod.LINE_REQ_DIR_IN)
time_nanosec2 = time.time_ns() 
print(lines.get_values())
time_nanosec = time.time_ns() 
delay =  time_nanosec - time_nanosec2

