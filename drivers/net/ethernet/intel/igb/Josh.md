The igb device:

igb_up - called when the device is brought up
igb_msix_ring - calls napi schedule
igb_poll is the callback for napi.

net_device_ops struct contains all the callbacks for a network device.


e1000_hw -> contains the hardware details.
igb_adapter - board specific data 
what is flash_address


https://www.reddit.com/r/homelab/comments/ewcsjb/notes_on_intel_82576_gigabit_adapters/