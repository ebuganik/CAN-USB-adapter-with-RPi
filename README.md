# Design and Implementation of a CAN-USB adapter using a Raspberry Pi device

## About the project
This repository is part of a practical project for an undergraduate thesis at the Faculty of Electrical Engineering in Banja Luka. The project aims to develop a device similar to PEAKCAN using a Raspberry Pi. The Raspberry Pi will receive and process CAN messages through a C++ application, then transmit them via an interface like USB to a PC, where they will be displayed using a Python application. Additionally, requests from the PC will be sent to read from the CAN bus and to write data onto the bus. 
**NOTE**: The repository is still under development.
> Add a picture of the main idea behind the project
## Raspberry Pi Setup and Hardware Interface Configuration
> Include the configuration of the Raspberry Pi device, covering both serial communication and connection to the hardware interface. Also, provide details on the connections used with the Raspberry Pi and the additional circuit designed for CAN communication.
## Setup Instructions and Launching the Applications
### Raspberry Pi (C++ Application)
After cloning the repository, the C++ application for the Raspberry Pi will be located in the ```rpi/project```  folder. This application is designed to handle CAN communication, as well as process requests that come over the serial port. Ensure that the Raspberry Pi is properly connected to the CAN network and configured to use CAN interface, while also setting up the serial connection for receiving and processing requests from external devices. This application interacts with the CAN network using the ``libsocketcan`` library so it's necessary that this library is previously installed, as it follows:
```
sudo apt-get install libsocketcan-dev
```
Once the library is installed, navigate to the rpi/project folder and use the Makefile to compile the application:
```
cd rpi/project
make
```
The Makefile will handle the compilation and linking of the libsocketcan library along with other dependencies. After successful compilation, the C++ application will run on the Raspberry Pi, enabling it to interact with the CAN bus after first write/read request from serial port. 
### Windows (Python Application)
The Python application, located in the ```windows``` folder, communicates with the Raspberry Pi via a serial connection. This application requires the ```pyserial``` library. To set it up and run it, follow these steps:
```
pip install pyserial
```
or for Python3:
```
pip3 install pyserial
```
Once the ```pyserial``` package is installed, navigate to the ```windows``` folder and execute the main Python script:
```
cd windows
python main.py
```





