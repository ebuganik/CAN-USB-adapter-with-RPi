# Design and Implementation of a CAN-USB adapter using a Raspberry Pi device

## About the project
This repository is a part of a practical project for an undergraduate thesis at the Faculty of Electrical Engineering in Banja Luka. The project aims to develop a device similar to PEAKCAN using a Raspberry Pi. The Raspberry Pi will receive and process CAN messages through a C++ application and then transmit them via an interface such as USB to a PC, where they will be displayed using a Python application. Also, requests from the PC will be sent to read from the CAN bus and to write data onto the bus. The following block diagram illustrates the main idea of the project.
**NOTE**: The repository is still under development.
<p align="center">
<img src="https://github.com/user-attachments/assets/3c8a1fa4-6ee8-49b5-8495-ba2b1135ac24"width = "750", height = "400">

## Raspberry Pi Setup and Hardware Interface Configuration
To use the CAN interface on the Raspberry Pi platform it is necessary to provide an appropriate hardware module that is connected to one of the interfaces offered by this platform. This project assignment involves designing the module using MCP2515 CAN controller and the MCP2551 CAN transceiver, which enable the connection of a microcontroller to the CAN network via the interface.

The MCP2515 CAN controller is already supported in the *Linux* operating system through a driver module. To ensure this module loads at system startup on the Raspberry Pi platform, it is necessary to modify the system's hardware configuration by specifying the appropriate parameters in the `/boot/config.txt` file. Therefore, this file should be edited using following command:
```
sudo nano /boot/config.txt
```
to add following lines:
```
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=16000000,spimaxfrequency=1000000,interrupt=25
```
To enable UART upon booting Raspberry Pi, add `enable_uart=1` at the end of the same file.

It's also useful to have serial console configured in case of testing serial communication (to check if serial cable works fine, for example). To do that, in this case it was necessary to have following line in `/boot/cmdline.txt`, previously opened with `sudo` privileges:
```
dwc_otg.lpm_enable=0 console=tty1 console=serial0, 115200, root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait 
```
> Add instructions how to connect Raspberry Pi with hardware module and MCP2551 to MCP2515
## Setup Instructions and Launching the Applications
> Getting started with G++
### Raspberry Pi (C++ Application)
After cloning the repository, the C++ application for the Raspberry Pi will be located in the `rpi/project`  folder. This application is designed to handle CAN communication, as well as process requests that come over the serial port. Ensure that the Raspberry Pi is properly connected to the CAN network and configured to use CAN interface, while also setting up the serial connection for receiving and processing requests from external devices. This application interacts with the CAN network using the *libsocketcan* library so it's necessary that this library is previously installed, as it follows:
```
sudo apt-get install libsocketcan-dev
```
Once the library is installed, navigate to the `rpi/project` folder and use the `Makefile` to compile the application:
```
cd rpi/project
make
```
The Makefile will handle the compilation and linking of the *libsocketcan* library along with other dependencies. It is also possible to change the name of the CAN interface by running 
```
make CAN_INTERFACE_NAME=can1
```
after making changes to the `/boot/config.txt` file.After successful compilation, the C++ application will run on the Raspberry Pi, enabling it to interact with the CAN bus after first write/read request from serial port. 
### Windows (Python Application)
The Python application, located in the `windows` folder, communicates with the Raspberry Pi via a serial connection. This application requires the *pyserial* library. To set it up and run it, follow these steps:
```
pip install pyserial
```
or for Python3:
```
pip3 install pyserial
```
Once the `pyserial` package is installed, navigate to the `windows` folder and execute the main Python script (with `python3` or just `python`)
```
cd windows
python main.py
```
Executing the Python application in this case means that the default parameters for configuring serial communication are set to `COM13`  serial port and a baud rate of `115200` , as was the case during the development of this project. If needed, these parameters can be modified by running the script in a way where command-line arguments are passed to the application, allowing the serial communication speed to be adjusted according to the available baud rates or another serial port. In that case, run `main.py` as following:
```
python main.py --port COM11 --baudrate 19200
```



