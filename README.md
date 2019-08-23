Driver for at24c32 using I2C protocol:
-------------------------------------

Device Info:
-------------
The AT24C32 provides 32,768(4KB) bit serial electrical erasable & programmable read only memory. allows up to 8 devices to share a common 2-wire bus.

As per device manual Device Address is:

	1  0  1  0  A2  A1  A0 r/w
       MSB		       LSB

so node reg property address is 0x50, and pagesize 32, address-width 12.


Device Node:
-------------
Device node has to be inserted into the device tree file for the specfic board. 

property compatible = "24c32"	: its specfies the device is compatible with driver with this compatible property.
reg 				: Device Address
size				: Total size of device
pagesize			: Page size 
address-width			: address-width of the device

driver has to been implemented according to this info. in mind.

Driver Stack:
----------------
##########################################################

      User-Space			//User Send info from here

-------------------------

     Kernel-abs (sysfs used for this device)

-------------------------

     I2C client (High Level Driver) 	//Our Driver Layer

-------------------------

     core layer(protocol)
 
-------------------------

    I2C controller
   
-------------------------

       I2C Device

############################################################

Working of Driver:
-------------------
1) i2c_device_id contain which is used to match the driver
2) Driver Have Been Registered with I2C driver Layer and i2c struct is passed during registration
3) When driver is registered it register it self with Middle Layer. 
   Middle(core) Layer check if any device struct with compatible "24c32" is present.
4) If compatible Propery is matched device probe func is invoked.
5) Device probe func does all the init. stuff and reterive all the device property.
6) device is also registerd with sysfs layer.
7) Files are created in /sys/at24_eeprom and user can use them to communicate to the device.
8) driver use i2c_transfer api to intract with the device the perform read and write ops.
