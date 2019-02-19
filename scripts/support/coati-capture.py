# Args:
# system app power(1/0)
import saleae as sal
import time
import os
import sys

s = sal.Saleae(host='localhost',port=10429)
print("connected")
try:
    s.set_performance(sal.PerformanceOption.Quarter)
    print("Set performance to Quarter.")
except s.CommandNAKedError:
    print("Could not set performance.")
    print("\tIs a physical Saleae device connected? This command only works")
    print("\twhen actual hardware is plugged in. You can skip it if you are")
    print("\tjust trying things out.")
devices = s.get_connected_devices()

print("Connected devices:")
for device in devices:
        print("\t{}".format(device))

# n.b. there are always a few connected test devices if no real HW
if len(devices) > 1:
        print("No physical saleae device connected!")
        sys.exit(1)
else:
        print("Only one Saleae device. Skipping device selection")

if s.get_active_device().type == 'LOGIC_4_DEVICE':
        print("Logic 4 does not support setting active channels; skipping")
else:
        # We'll test if this works event with analog empty
        digital = [0,5,6,7]
        analog = []
        print("Setting active channels (digital={}, \
                analog={})".format(digital, analog))
        s.set_active_channels(digital, analog)
        print("Setting trigger channels\n")
        s.set_trigger_one_channel(0,sal.Trigger.Posedge)

digital, analog = s.get_active_channels()
print("Reading back active channels:")
print("\tdigital={}\n\tanalog={}".format(digital, analog))

print("Setting to sample rate to at least digitial 8 MS/s, analog 625k S/s")
rate = s.set_sample_rate_by_minimum(8e6)
print("\tSet to", rate)
print("Setting collection time to:")
sec = 0
if ( sys.argv[3] == "cont" ):
    sec = 40
else:
    if ( sys.argv[2] == "rsa" or sys.argv[2] == "ar"):
        sec = 250
    else:
        sec = 200
print("\t", sec)

s.set_capture_seconds(sec)

# Set time format
ID = time.strftime('%m-%d--%H-%M')
system = sys.argv[1]
system_path = '/tmp/saleae_traces/' + system + '/' + sys.argv[3] + '/'
sys_path_partial = '/tmp/saleae_traces/' + system + '/'
app = sys.argv[2]
# Make system directory if it doesn't exist
if not os.path.exists(sys_path_partial):
    os.mkdir(sys_path_partial)
if not os.path.exists(system_path):
    os.mkdir(system_path)
path = os.path.abspath(os.path.join(system_path,system + "_" + app + "_" + ID))
print(path)

print("Built path!\n")
# Start the capture
s.capture_start()
while not s.is_processing_complete():
        print("\t..waiting for capture to complete")
        time.sleep(1)
print("Capture complete")
s.export_data2(path,digital_channels=[0,5,6,7],analog_channels=None,time_span=None,format='csv',display_base='separate')
print("Done capture and writing")


