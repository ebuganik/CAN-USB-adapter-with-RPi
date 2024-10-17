# Design and Implementation of a CAN-USB adapter using a Raspberry Pi device

## About the project
This repository is part of an undergraduate thesis project at the Faculty of Electrical Engineering in Banja Luka. The project involves developing a device similar to PCAN-USB using a Raspberry Pi. Requests are sent from the PC via a Python application to the Raspberry Pi, where they are processed by a C++ application. These requests initiate operations such as reading from the CAN bus or sending data to CAN bus. The processed CAN messages are then transmitted back to the PC through an interface like USB and displayed in the Python application. The block diagram below illustrates the core concept of the project.

<p align="center">
<img src="https://github.com/user-attachments/assets/3c8a1fa4-6ee8-49b5-8495-ba2b1135ac24"width = "750", height = "400">

### Requirements
- Raspberry Pi 3B target platform with pre-installed Raspbian operating system and internet connectivity
- Power adapter for Raspberry Pi
- USB to Ethernet Adapter
- Breadboard for implementing the electrical circuit.
- Additional electronic components:
  -  MCP2515
  -  MCP2551
  -  Resistors for voltage division: 10 kΩ and 22 kΩ
  -  Pull-up resistors: 4.7 kΩ
  -  Terminal resistor: 120 Ω
  -  Capacitors of 0.1 uF for stabilizing the power supply and of 22 pF for the quartz crystal on the microcontroller's oscillator
  - Wires for connections
    
- USB to TTL Serial 3.3V Adapter Cable for serial communication
- PCAN-USB (optional)

## Raspberry Pi and Hardware Interface Setup
### Initial setup of Raspberry Pi 
Since the Ethernet adapter has its own IP address, it was also necessary to set the IP address of the Raspberry Pi device in the `/etc/dhcpcd.conf` file with `sudo` privileges, by adding the following lines:
```
interface <NETWORK>
static ip_address=<STATICIP>/24
static routers=<ROUTERIP>
static domain_name_servers=<DNSIP>
```
**Note:** Keywords enclosed in angle brackets (<>) should be replaced with appropriate values for your network configuration. This ensures that Raspberry Pi has a static IP address and can properly route traffic through the specified router and DNS server. In this particular case ROUTERIP and DNSIP were set to USB to Ethernet Adapter IP address, NETWORK to `eth0` and Raspberry Pi's STATICIP was assigned to the first IP address in the ROUTERIP network.

To use the CAN interface on the Raspberry Pi platform it is necessary to provide an appropriate hardware module that is connected to one of the interfaces offered by this platform. This project assignment involves designing the module using MCP2515 CAN controller and the MCP2551 CAN transceiver, which enable the connection of a microcontroller to the CAN network via the interface.

The MCP2515 CAN controller is already supported in the *Linux* operating system through a driver module. To ensure this module loads at system startup on the Raspberry Pi platform, it is necessary to modify the system's hardware configuration by specifying the appropriate parameters in the `/boot/config.txt` file. Therefore, this file should be edited using following command:
```
sudo nano /boot/config.txt
```
to add following lines (for *Linux* kernel versions >= 4.4.x):
```
dtparam=spi=on
dtoverlay=mcp2515-can0,oscillator=16000000,spimaxfrequency=1000000,interrupt=25
#dtoverlay=mcp2515-can1,oscillator=16000000,spimaxfrequency=1000000,interrupt=24
```
**Note:** Linux kernel version can be checked with command `uname -r`.

Just like with SPI, to enable UART upon booting Raspberry Pi, add `enable_uart=1` at the end of the same file. The commented line of code should be uncommented if we want to enable more than one communication channel, in this case, we could have `can0` and `can1`. Connections between the components must be established, what will be explained in the following sections. Afterwards, it is useful to check if MCP2515 CAN controller is succesfully initialized, especially if we plan to work with both channels:
```
dmesg | grep can
[   39.846066] mcp251x spi0.1 can0: MCP2515 successfully initialized.
[   39.882300] mcp251x spi0.0 can1: MCP2515 successfully initialized.
```
It's also helpful to have the serial console configured for testing serial communication (for example, to verify if the USB to UART cable is functioning properly). In this setup, the baudrate is set to 115200, which should be matched when opening a session in [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) (to connect via PuTTY, select the appropriate COM port corresponding to your USB to UART adapter, too). To achieve this, the following  needs to be added to `/boot/cmdline.txt`, which should be opened with `sudo` privileges:
```
dwc_otg.lpm_enable=0 console=tty1 console=serial0, 115200, root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline fsck.repair=yes rootwait 
```
**Note:** After applying any changes to the system files, emember to run `sudo reboot` to ensure that all modifications are properly saved and take effect.
### Raspberry Pi and Hardware connections
According to the conceptual block diagram, the Raspberry Pi must be connected to the MCP2515 microcontroller to interface with the CAN bus via the MCP2515 transceiver. Additionally, a suitable solution is required to establish serial communication between the Raspberry Pi and the PC. The next step involves reviewing the Raspberry Pi pins that are compatible with the protocols outlined in the diagram. The following image highlights the Raspberry Pi pins used for this project's specific connections.
<p align="center">
<img src = "https://github.com/user-attachments/assets/75d73bf0-53fb-4369-afc7-87fa7d1f9be5" width = "750, height = "250">
  
| Raspberry Pi Pin | USB to TTL Serial 3.3V Adapter Cable |   
| :------: | :------: | 
| GPIO 14 (TXD) | RXD| 
| GPIO 15 (RXD) | TXD| 
| Ground | GND |

Following image illustrates the basic idea of master-slave principle in SPI communication. Both peripherals are connected in parallel on the same SPI bus (MOSI, MISO, SCLK), with each device having its own unique CE line (CE0, CE1). This allows the Raspberry Pi to select which device to communicate with, enabling multiple CAN communication channels on the Raspberry Pi to interact with different CAN networks simultaneously.

<p align="center">
<img src ="https://github.com/user-attachments/assets/3c9c87ea-3776-41a9-9010-f8b97f8f4161" width = "550, height = "250">

## Required installations and Launching the Applications
The C++ application can be compiled directly on the Raspberry Pi device. For the purposes of this project, the code was edited using Visual Studio Code, which allows for connection to the Raspberry Pi via SSH. This enables remote development and easy management of files on the Raspberry Pi. While it is also possible to perform the compilation using a `Makefile`, it is essential to first verify that the necessary `g++` compiler is available.

Ensure your Raspberry Pi is up-to-date with the latest software and security patches.
```
sudo apt-get update
sudo apt-get upgrade
```
To compile and run C++ programs on your Raspberry Pi, you need to install the GNU Compiler Collection (GCC), which includes the g++ compiler for C++.

Install g++ and verify the installation:
```
sudo apt-get install g++
g++ --version
```
### Raspberry Pi (C++ Application)
After cloning the repository, the C++ application for the Raspberry Pi will be located in the `rpi/project`  folder. This application is designed to handle CAN communication, as well as process requests that come over the serial port. Ensure that the Raspberry Pi is properly connected to the CAN network and configured to use CAN interface, while also setting up the serial connection for receiving and processing requests from external devices. This application interacts with the CAN network using the *libsocketcan* library so it's necessary that this library is previously installed, as it follows:
```
sudo apt-get install libsocketcan-dev
```
Additionally, the application utilizes the *wiringpi* library to control the LED during the execution of read, write, and periodic write operations, which also requires necessary installation. In this repository, *wiringpi* was located in `rpi/project/ext_lib` and included in both the C++ application files and the Makefile. Here are the steps to successfully clone and build the *wiringpi* content to link with this application:
```
git clone https://github.com/WiringPi/WiringPi
cd WiringPi
./build
```

Once the library is installed, navigate to the `rpi/project` folder and use the `Makefile` to compile the application:
```
cd rpi/project
make run
```
The Makefile will manage the compilation and linking of the `libsocketcan` and `wiringpi` libraries along with any additional dependencies, taking `can0` as the CAN interface name by default. If an additional SPI device (MCP2515) is connected to the Raspberry Pi's SPI0 pins as previously explained, after uncommenting the specified line in the `/boot/config.txt` file (don't forget to `sudo reboot` again), you can invoke the make command as follows:

```
make run CAN_INTERFACE_NAME=can1
```
After successful compilation, the C++ application will run on the Raspberry Pi, enabling it to interact with the CAN bus after first write/read request from serial port. 
### PCAN View 
For testing purposes, a PCAN-USB device is used as a node connected to CAN bus CAN-H and CAN-L to read sent requests from serial or to send new one to serial. If device is available, download the latest version of PEAKCAN View application for Windows from [PEAK System](https://www.peak-system.com/?&L=1) and run the installer. Start the PEAKCAN View, configure the CAN interface and intended bitrate to start communication.

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
Executing the Python application means that the default parameters for configuring serial communication are set to the `COM13` serial port and a baud rate of `115200`, as used during the development of this project. If needed, these parameters can be modified by running the script with command-line arguments, allowing the serial communication speed to be adjusted according to the available baud rates or a different serial port, where, additionally, application includes checks for various baudrates and active system ports. In that case, run `main.py` as follows:

```
python main.py --port COM11 --baudrate 19200
```



