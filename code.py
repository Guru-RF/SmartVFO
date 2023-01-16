import rotaryio
import board
import digitalio
import adafruit_rgbled
import simpleio
import time

RGBled1 = adafruit_rgbled.RGBLED(board.GP3, board.GP4, board.GP5, invert_pwm=True)

RGBled1.color = (255, 0, 0)
time.sleep(2)
RGBled1.color = (0, 255, 0)
time.sleep(2)
RGBled1.color = (0, 0, 255)
time.sleep(2)

encoder = rotaryio.IncrementalEncoder(board.GP17, board.GP16)

buttonENC= digitalio.DigitalInOut(board.GP19)
buttonENC.direction = digitalio.Direction.INPUT
buttonENC.pull = digitalio.Pull.UP

buttonTOP= digitalio.DigitalInOut(board.GP15)
buttonTOP.direction = digitalio.Direction.INPUT
buttonTOP.pull = digitalio.Pull.UP

#buttonBOT= digitalio.DigitalInOut(board.GP18)
#buttonBOT.direction = digitalio.Direction.INPUT
#buttonBOT.pull = digitalio.Pull.UP

last_position = None
while True:
    position = encoder.position
    if last_position is None or position != last_position:
        print(position)
	print(buttonENC.value)
	print(buttonTOP.value)
	#print(buttonBOT.value)
    last_position = position
