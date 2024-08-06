import rotaryio
import board
import digitalio
import adafruit_rgbled
import simpleio
import time
import usb_midi
import adafruit_midi
from adafruit_midi.note_off import NoteOff
from adafruit_midi.note_on import NoteOn
from rainbowio import colorwheel

usb_midi.set_names(streaming_interface_name="SmartVFO", audio_control_interface_name="SmartVFO", in_jack_name="SmartVFO", out_jack_name="SmartVFO")

# setup midi
midi = adafruit_midi.MIDI(
        midi_in=usb_midi.ports[0], in_channel=0, midi_out=usb_midi.ports[1], out_channel=0 )

RGBled1 = adafruit_rgbled.RGBLED(board.GP3, board.GP4, board.GP5, invert_pwm=True)
RGBled1.color = (0, 0, 255)

encoder = rotaryio.IncrementalEncoder(board.GP17, board.GP16)

buttonENC= digitalio.DigitalInOut(board.GP19)
buttonENC.direction = digitalio.Direction.INPUT
buttonENC.pull = digitalio.Pull.UP

buttonTOP= digitalio.DigitalInOut(board.GP18)
buttonTOP.direction = digitalio.Direction.INPUT
buttonTOP.pull = digitalio.Pull.UP

buttonBOT= digitalio.DigitalInOut(board.GP15)
buttonBOT.direction = digitalio.Direction.INPUT
buttonBOT.pull = digitalio.Pull.UP

last_position = 0
wheel = 0
wait = False
waittimer = 0
while True:
    position = encoder.position
    if position != last_position:
        if position > last_position:
            # up
            wait = True
            if wheel is 255:
                wheel = 0
            wheel=wheel+1
            RGBled1.color = colorwheel(wheel)
            midi.send(NoteOn(30,0))
            midi.send(NoteOff(30,0))
        elif position < last_position:
            # down
            wait = True
            if wheel is 0:
                wheel = 255
            wheel=wheel-1
            RGBled1.color = colorwheel(wheel)
            midi.send(NoteOn(31,0))
            midi.send(NoteOff(31,0))
        last_position = position
    if wait is False:
        RGBled1.color = (0, 0, 255)
    if buttonENC.value is False:
        RGBled1.color = (255, 0, 0)
        midi.send(NoteOn(32,0))
        midi.send(NoteOff(32,0))
        time.sleep(0.5)
    if buttonBOT.value is False:
        RGBled1.color = (255, 0, 0)
        midi.send(NoteOn(33,0))
        midi.send(NoteOff(33,0))
        time.sleep(0.5)
    if buttonTOP.value is False:
        RGBled1.color = (255, 0, 0)
        midi.send(NoteOn(34,0))
        midi.send(NoteOff(34,0))
        time.sleep(0.5)
    if wait is True:
        waittimer = waittimer + 1
        if waittimer is 10000:
            waittimer = 0
            wait = False
