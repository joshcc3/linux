The igb device:

igb_up - called when the device is brought up
igb_msix_ring - calls napi schedule
igb_poll is the callback for napi.

net_device_ops struct contains all the callbacks for a network device.


e1000_hw -> contains the hardware details.
igb_adapter - board specific data 
what is flash_address


https://www.reddit.com/r/homelab/comments/ewcsjb/notes_on_intel_82576_gigabit_adapters/


Reasons why the driver is bad - doesn't allow you to customize writes.
doesn't allow you to congfigure low latency interrupts.
even if dca is enabled it still calls out to dma which requires a cache snoop. 


==== 
physics stuff:
so correct my understanding if i'm wrong about htrow transistors work. there are two types of materials n type and p type. n type has free electrons on it and p type has holes. these materials are created by doping silicon with phosphorus and boron respectively.
a tranistor consists of a 3 layers, an emittor a base a collector. the base is sandwiched between the two and the materials of each are npn or pnp. in the context of npn, the free electrons jump to the base creating an electric field preventing movement to the negatively charged base. apply a potential difference across the sides of the base allows one to direct free electrons from the emittor to collector or not. this is typically how transistors work.
the base is a semiconductor material. the theory of semiconductor materials is explained by band theory. there is a valence band, at these levels electrons are stuck in orbit and cannot move through the crystal lattice of a material. in the conduction band they can. conductors have an overlapping bands. in semiconductors the gap is small and energy can incite the electrons to jump to their conduction bands.
Silicon atoms contains 8 valence electrons, in the silicon structure each silicon atom shares an electron with 4 other neighbors. 
a description of the lattice structure: https://www.youtube.com/watch?v=PvnxhwaAq_Y consists of 12 repeating cubic shapes with 4 corner atoms arranged in a cube (that are missing one bond. 4 inner atoms that have all bonds satisfied
and 4 other outer ones which have 2 satisfied.

mosfet stands for metal oxide semi fet..