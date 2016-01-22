# b2g


## Overview
This project allows to control a system composed  with a lithium-ion battery and an inverter charger to 
maximize the consumption of the energy generated with the sun (Photovoltaics).  

The system is connected to the network and is able to receive API commands.

Sending the command

`IPdress/battery?params= followed by the disired param`
is possible o change the desired output or input power [W]  

The schematic of the system is [here](https://github.com/vepascal/b2g/blob/master/docs/Scematico%20Arduino%20con%20connessioni.pdf)
 
### Example 
In my case:
- carge at 241 W: `195.176.65.221/battery?params=241`
- discarge/inject at 333 W: `195.176.65.221/battery?params=-333`
- idle mode 0 W: `195.176.65.221/battery?params=0`
 
## Contents
### Codes
In src you find the Arduino code

### Libraries
In lib you find all the needed libraries.
Only \Lib\studer\scom_port_c99.h has been modified.
The file \Lib\studer\extender.h has been created with some functionalities of the inverter.

### Attachment
In docs you find all the datasheet and schematics of the components



## Hardware
### Inverter charger
- [Xtender Studer XTM2400-24](http://www.studer-innotec.com/?cat=sine_wave_inverter-chargers&id=432)
- [Communication module Xcom-232i - Studer Innotec](http://www.studer-innotec.com/?cat=sine_wave_inverter-chargers&id=432&aId=1418&tab=4)

### Battery pack
- [Clayton Power lithium-ion (Model no.: 012-00002GF)](http://www.claytonpower.com/products/lithium-ion-batteries/)


### Control Hardware
- [Arduino Uno - R3 SMD](https://www.sparkfun.com/products/11224)
- [CAN-BUS Shield](https://www.sparkfun.com/products/13262)
- [Arduino Ethernet Shield 2](https://www.sparkfun.com/products/11166)
- [SparkFun RS232 Shifter - SMD](https://www.sparkfun.com/products/449)
- [RS232 Shield V2](https://www.sparkfun.com/products/13029)

