TARGET = main
CXX = g++
LIBS = -lsocketcan -L../ext_lib/WiringPi/wiringPi -lwiringPi
CXXFLAGS = -Wall -g -DCAN_INTERFACE_NAME="$(CAN_INTERFACE_NAME)"
THREAD_FLAGS = -pthread
SRCS = main.cpp canhandler.cpp serial.cpp
OBJS = $(SRCS:.cpp=.o)

# Set capabilities
SUDO_SET_CAPS = sudo setcap cap_net_raw,cap_net_admin+ep ${TARGET}

# If CAN interface name not given, use can0 by default
CAN_INTERFACE_NAME ?= can0

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

# Clean rule to remove generated files
.PHONY: clean
clean:
	\rm -f $(TARGET) $(OBJS)
