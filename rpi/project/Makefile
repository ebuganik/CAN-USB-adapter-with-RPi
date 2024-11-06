TARGET = rpi_pcan_bin
CXX = g++
LIBS = -lsocketcan -L../ext_lib/rpi_lib/WiringPi/wiringPi -lwiringPi
CXXFLAGS = -Wall -g -I../ext_lib/json_lib/single_include -I../ext_lib/rpi_lib/wiringPi/wiringPi
THREAD_FLAGS = -pthread
SRCS = main.cpp canhandler.cpp serial.cpp
OBJS = $(SRCS:.cpp=.o)

# Set capabilities
SUDO_SET_CAPS = sudo setcap cap_net_raw,cap_net_admin+ep ${TARGET}

# If CAN interface name not given, use can0 by default
CAN_INTERFACE_NAME ?= can0

# Add another CAN interface name if necessary
CXXFLAGS+=-DCAN_INTERFACE_NAME="$(CAN_INTERFACE_NAME)"

all: $(TARGET)

# Rule to link object files into the target executable
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) -c $< -o $@

# Default rule to build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) -o $(TARGET) $(OBJS) $(LIBS)

setcaps:
	$(SUDO_SET_CAPS)

run: $(TARGET) setcaps
	sudo ./$(TARGET) $(CAN_INTERFACE_NAME)

service_install_run: $(TARGET)
	sudo cp ./rpi_pcan.service /etc/systemd/system
	sudo systemctl enable rpi_pcan.service
	sudo systemctl start rpi_pcan.service

# Clean rule to remove generated files
.PHONY: clean
clean:
	\rm -f $(TARGET) $(OBJS)
