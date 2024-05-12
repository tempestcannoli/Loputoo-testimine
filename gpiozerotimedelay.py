from gpiozero import MCP3008, PWMLED
import time

potentiometer = MCP3008(channel=0)  # Connect potentiometer to channel 0
#led = PWMLED(17)  # Connect LED to GPIO pin 17
i = 0
while (i < 110):
    current_time_ns = time.time_ns()
    value = potentiometer.value  # Read analog value (0.0 to 1.0)
    #print(value)
    current_time_ns2 = time.time_ns()
    delay = current_time_ns2 - current_time_ns
    print(delay)
    i = i + 1  
    #led.value = value  # Set LED brightness
    #sleep(0.1)  # Pause for a short time
