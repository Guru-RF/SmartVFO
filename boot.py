import storage
import supervisor
import usb_cdc
import usb_midi


supervisor.set_usb_identification("RF.Guru", "SmartVFO")

usb_midi.set_names(streaming_interface_name="SmartVFO", audio_control_interface_name="SmartVFO", in_jack_name="SmartVFO", out_jack_name="SmartVFO")
usb_midi.enable()

usb_cdc.enable(console=False, data=True)

new_name = "SmartVFO"
storage.remount("/", readonly=False)
m = storage.getmount("/")
m.label = new_name
storage.remount("/", readonly=True)
