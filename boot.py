import board
import digitalio
import storage
import supervisor
import usb_cdc
import usb_midi

supervisor.set_usb_identification("RF.Guru", "SmartVFO")

usb_midi.set_names(
    streaming_interface_name="SmartVFO",
    audio_control_interface_name="SmartVFO",
    in_jack_name="SmartVFO",
    out_jack_name="SmartVFO",
)
usb_midi.enable()

buttonENC = digitalio.DigitalInOut(board.GP19)
buttonENC.direction = digitalio.Direction.INPUT
buttonENC.pull = digitalio.Pull.UP

usb_cdc.enable(console=False, data=True)

if buttonENC.value is True:
    storage.disable_usb_drive()
    storage.remount("/", readonly=False)
else:
    new_name = "SmartVFO"
    storage.remount("/", readonly=False)
    m = storage.getmount("/")
    m.label = new_name
    storage.remount("/", readonly=True)
