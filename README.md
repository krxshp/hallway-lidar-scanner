# hallway-lidar-scanner

This project uses a Cortex-M4 microcontroller, a time-of-flight sensor, and a stepper motor to generate a 3D point-cloud map of a hallway. It demonstrates the core principles of LiDAR using a significantly more affordable hardware setup.


## Hardware

* 1x Texas Instruments MSP432E401Y with a Cortex M4F
* 1x VL53L1X ToF Sensor
* 1x VMA401 Stepper Motor
* 1x ULN2003 Driver Board
* 2x 4 Legged Tact Switches (optional, can otherwise use onboard buttons)
* Various lengths of Dupont wires

## Software

* Keil µVision5 - used to write, build, and flash the microcontroller firmware
* VL53L1X Driver Library
* MATLAB
* C, C++

## Notes

* Mount the ToF sensor securely and leave enough slack in its wires for a complete 360° rotation.
* The motor automatically unwinds after every scan to prevent cable tangling. This can be avoided with a slip coupling if faster speed is desired.
* The system records 32 measurements per rotation, with one measurement every 11.25°. This can also be changed depending on desired speed and quality. More info can be found in the device’s datasheet.
* Update the COM port in the MATLAB script if the board is not detected on the default port.
* A wiring diagram can be found at the bottom of the datasheet.
