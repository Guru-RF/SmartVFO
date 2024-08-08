import asyncio
import time

import adafruit_midi
import adafruit_rgbled
import board
import digitalio
import pwmio
import rotaryio
import supervisor
import usb_cdc
import usb_midi
from adafruit_midi.note_off import NoteOff
from adafruit_midi.note_on import NoteOn
from microcontroller import watchdog as w
from rainbowio import colorwheel
from watchdog import WatchDogMode

import config

# stop autoreloading
supervisor.runtime.autoreload = False

# configure watchdog
w.timeout = 2
w.mode = WatchDogMode.RESET
w.feed()

# User config
WPM = config.WPM
if not WPM:
    WPM = 15
SIDEFREQ = config.SIDEFREQ
if not SIDEFREQ:
    SIDEFREQ = 880

# cw
OFF = 0
ON = 2**15

# setup buzzer (set duty cycle to ON to sound)
buzzer = pwmio.PWMOut(board.GP24, variable_frequency=True)
buzzer.frequency = SIDEFREQ

usb_midi.set_names(
    streaming_interface_name="SmartVFO",
    audio_control_interface_name="SmartVFO",
    in_jack_name="SmartVFO",
    out_jack_name="SmartVFO",
)

# setup midi
midi = adafruit_midi.MIDI(
    midi_in=usb_midi.ports[0], in_channel=0, midi_out=usb_midi.ports[1], out_channel=0
)

RGBled1 = adafruit_rgbled.RGBLED(board.GP3, board.GP4, board.GP5, invert_pwm=True)
RGBled1.color = (0, 255, 0)

encoder = rotaryio.IncrementalEncoder(board.GP17, board.GP16)

buttonENC = digitalio.DigitalInOut(board.GP19)
buttonENC.direction = digitalio.Direction.INPUT
buttonENC.pull = digitalio.Pull.UP

buttonTOP = digitalio.DigitalInOut(board.GP18)
buttonTOP.direction = digitalio.Direction.INPUT
buttonTOP.pull = digitalio.Pull.UP

buttonBOT = digitalio.DigitalInOut(board.GP15)
buttonBOT.direction = digitalio.Direction.INPUT
buttonBOT.pull = digitalio.Pull.UP

# setup usb serial
serial = usb_cdc.data

# setup paddle inputs
dit_key = digitalio.DigitalInOut(board.GP13)
dit_key.direction = digitalio.Direction.INPUT
dit_key.pull = digitalio.Pull.UP
dah_key = digitalio.DigitalInOut(board.GP12)
dah_key.direction = digitalio.Direction.INPUT
dah_key.pull = digitalio.Pull.UP


# timing
def dit_time():
    global WPM
    PARIS = 50
    return 60.0 / WPM / PARIS


# setup encode and decode
encodings = {}


def encode(char):
    global encodings
    if char in encodings:
        return encodings[char]
    elif char.lower() in encodings:
        return encodings[char.lower()]
    else:
        return ""


decodings = {}


def decode(char):
    global decodings
    if char in decodings:
        return decodings[char]
    else:
        # return '('+char+'?)'
        return "¿"


def MAP(pattern, letter):
    decodings[pattern] = letter
    encodings[letter] = pattern


MAP("_", " ")
MAP(".-", "a")
MAP("-...", "b")
MAP("-.-.", "c")
MAP("-..", "d")
MAP(".", "e")
MAP("..-.", "f")
MAP("--.", "g")
MAP("....", "h")
MAP("..", "i")
MAP(".---", "j")
MAP("-.-", "k")
MAP(".-..", "l")
MAP("--", "m")
MAP("-.", "n")
MAP("---", "o")
MAP(".--.", "p")
MAP("--.-", "q")
MAP(".-.", "r")
MAP("...", "s")
MAP("-", "t")
MAP("..-", "u")
MAP("...-", "v")
MAP(".--", "w")
MAP("-..-", "x")
MAP("-.--", "y")
MAP("--..", "z")

MAP(".----", "1")
MAP("..---", "2")
MAP("...--", "3")
MAP("....-", "4")
MAP(".....", "5")
MAP("-....", "6")
MAP("--...", "7")
MAP("---..", "8")
MAP("----.", "9")
MAP("-----", "0")

MAP(".-.-.-", ".")  # period
MAP("--..--", ",")  # comma
MAP("..--..", "?")  # question mark
MAP("-...-", "=")  # equals, also /BT separator
MAP("-....-", "-")  # hyphen
MAP("-..-.", "/")  # forward slash
MAP(".--.-.", "@")  # at sign

MAP("-.--.", "(")  # /KN over to named station
MAP(".-.-.", "+")  # /AR stop (end of message)
MAP(".-...", "&")  # /AS wait
MAP("...-.-", "|")  # /SK end of contact
MAP("...-.", "*")  # /SN understood
MAP(".......", "#")  # error


# send to computer
async def send(c):
    await asyncio.sleep(0)
    if serial is not None:
        if serial.connected:
            serial.write(str.encode(c))


async def led(what):
    await asyncio.sleep(0)
    if what == "dit":
        RGBled1.color = (255, 0, 0)
    if what == "dah":
        RGBled1.color = (0, 255, 0)


# key down and up
async def cw(on):
    if on:
        midi.send(NoteOn(65, 0))
        buzzer.duty_cycle = ON
    else:
        midi.send(NoteOff(65, 0))
        buzzer.duty_cycle = OFF


# transmit pattern
async def play(pattern):
    for sound in pattern:
        if sound == ".":
            await led("dit")
            await cw(True)
            await asyncio.sleep(dit_time())
            await cw(False)
            await asyncio.sleep(dit_time())
        elif sound == "-":
            await led("dah")
            await cw(True)
            await asyncio.sleep(3 * dit_time())
            await cw(False)
            await asyncio.sleep(dit_time())
        else:
            await asyncio.sleep(4 * dit_time())
    await asyncio.sleep(2 * dit_time())


# receive, send, and play keystrokes from computer
async def serials():
    if serial is not None:
        if serial.connected:
            if serial.in_waiting > 0:
                raw = serial.in_waiting
                while raw:
                    letters = serial.read(raw).decode("utf-8")
                    for char in letters:
                        await send(char)
                        await play(encode(char))
                    raw = serial.in_waiting


# decode iambic
class Iambic:
    def __init__(self, dit_key, dah_key):
        self.dit_key = dit_key
        self.dah_key = dah_key
        self.dit = False
        self.dah = False
        self.SPACE = 0
        self.DIT = 1
        self.DIT_WAIT = 2
        self.DAH = 3
        self.DAH_WAIT = 4
        self.state = self.SPACE
        self.in_char = False
        self.in_word = False
        self.start = 0
        self.char = ""
        return None

    def hack(self):
        self.start = time.monotonic()

    def elapsed(self):
        return time.monotonic() - self.start

    def set_state(self, new_state):
        self.hack()
        self.state = new_state

    async def latch_paddles(self):
        if not self.dit_key.value:
            self.dit = True
        if not self.dah_key.value:
            self.dah = True

    async def start_dit(self):
        self.dit = False
        self.dah = False
        self.in_char = True
        self.in_word = True
        self.char += "."
        await led("dit")
        await cw(True)
        self.set_state(self.DIT)

    async def start_dah(self):
        self.dit = False
        self.dah = False
        self.in_char = True
        self.in_word = True
        self.char += "-"
        await led("dah")
        await cw(True)
        self.set_state(self.DAH)

    async def cycle(self):
        await self.latch_paddles()
        if self.state == self.SPACE:
            if self.dit:
                await self.start_dit()
            elif self.dah:
                await self.start_dah()
            elif self.in_char and self.elapsed() > 2 * dit_time():
                self.in_char = False
                await send(decode(self.char))
                self.char = ""
            elif self.in_word and self.elapsed() > 6 * dit_time():
                self.in_word = False
                await send(" ")
        elif self.state == self.DIT:
            if self.elapsed() > dit_time():
                await cw(False)
                self.dit = False
                self.set_state(self.DIT_WAIT)
        elif self.state == self.DIT_WAIT:
            if self.elapsed() > dit_time():
                if self.dah:
                    await self.start_dah()
                elif self.dit:
                    await self.start_dit()
                else:
                    self.set_state(self.SPACE)
        elif self.state == self.DAH:
            if self.elapsed() > 3 * dit_time():
                await cw(False)
                self.dah = False
                self.set_state(self.DAH_WAIT)
        elif self.state == self.DAH_WAIT:
            if self.elapsed() > dit_time():
                if self.dit:
                    await self.start_dit()
                elif self.dah:
                    await self.start_dah()
                else:
                    self.set_state(self.SPACE)


async def iambic_runner(iambic, w):
    print("Iambic task")
    while True:
        await iambic.cycle()
        await asyncio.sleep(0)


async def vfo_runner(w):
    print("VFO task")
    last_position = 0
    wheel = 0
    # wait = False
    # waittimer = 0
    start = time.monotonic()
    jumpstart = time.monotonic()
    speedjump = 25
    jumped = False
    while True:
        w.feed()
        await asyncio.sleep(0)
        position = encoder.position
        if jumped is True:
            jumpelapsed = time.monotonic() - jumpstart
            if jumpelapsed > 0.03:
                speedjump = speedjump * 3
            if speedjump > 1000:
                speedjump = 1000
            if jumpelapsed > 0.5:
                speedjump = 25
            jumpstart = time.monotonic()
        if position != last_position:
            speed = 0

            elapsed = time.monotonic() - start
            if elapsed < 0.01:
                speed = speedjump
                print(str(speed))
                jumped = True
            # else:
            # jumped = False
            start = time.monotonic()

            if position > last_position:
                if wheel == 255:
                    wheel = 0
                wheel = wheel + 1
                RGBled1.color = colorwheel(wheel)
                if speed > 0:
                    for _i in range(1, speed):
                        midi.send(NoteOn(30, 0))
                        midi.send(NoteOff(30, 0))
                else:
                    midi.send(NoteOn(30, 0))
                    midi.send(NoteOff(30, 0))
            elif position < last_position:
                if wheel == 0:
                    wheel = 255
                wheel = wheel - 1
                RGBled1.color = colorwheel(wheel)
                if speed > 0:
                    for _i in range(1, speed):
                        midi.send(NoteOn(31, 0))
                        midi.send(NoteOff(31, 0))
                else:
                    midi.send(NoteOn(31, 0))
                    midi.send(NoteOff(31, 0))
            last_position = position
        else:
            jumped = False
        if buttonENC.value is False:
            RGBled1.color = (255, 0, 0)
            midi.send(NoteOn(32, 0))
            midi.send(NoteOff(32, 0))
            await asyncio.sleep(0.5)
        if buttonBOT.value is False:
            RGBled1.color = (255, 0, 0)
            midi.send(NoteOn(33, 0))
            midi.send(NoteOff(33, 0))
            await asyncio.sleep(0.5)
        if buttonTOP.value is False:
            RGBled1.color = (255, 0, 0)
            midi.send(NoteOn(34, 0))
            midi.send(NoteOff(34, 0))
            await asyncio.sleep(0.5)


async def paddle_runner():
    print("Paddle task")
    if config.CW is False:
        while True:
            await asyncio.sleep(0)
            if dit_key.value is False:
                RGBled1.color = (0, 255, 0)
                midi.send(NoteOn(35, 0))
            else:
                midi.send(NoteOff(35, 0))
            if dah_key.value is False:
                RGBled1.color = (0, 0, 255)
                midi.send(NoteOn(36, 0))
            else:
                midi.send(NoteOff(36, 0))


async def serials_runner():
    print("Serials task")
    while True:
        await serials()
        await asyncio.sleep(0)


async def main():
    if config.CW is True:
        iambic = Iambic(dit_key, dah_key)
        iambic_task = asyncio.create_task(iambic_runner(iambic, w))
        serials_task = asyncio.create_task(serials_runner())
        vfo_task = asyncio.create_task(vfo_runner(w))
        await asyncio.gather(iambic_task, serials_task, vfo_task)
    else:
        serials_task = asyncio.create_task(serials_runner())
        paddle_task = asyncio.create_task(paddle_runner())
        vfo_task = asyncio.create_task(vfo_runner(w))
        await asyncio.gather(serials_task, vfo_task, paddle_task)


asyncio.run(main())
