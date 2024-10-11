TARGET = main
CXX = g++
CAN_INTERFACE_NAME = can1
LIBS = -lsocketcan -L../ext_lib/WiringPi/wiringPi -lwiringPi
CXXFLAGS = -Wall -g 
THREAD_FLAGS = -pthread
SRCS = main.cpp canhandler.cpp serial.cpp
OBJS = $(SRCS:.cpp=.o)

# Set capabilities
SUDO_SET_CAPS = sudo setcap cap_net_raw,cap_net_admin+ep ${TARGET}

# Overriding default CAN interface name with a new one
ifneq ($(CAN_INTERFACE_NAME),) 
	CXXFLAGS+=-DCAN_INTERFACE_NAME="$(CAN_INTERFACE_NAME)"
endif

all:$(TARGET) run

# Rule to link object files into the target executable
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) -c $< -o $@ 

# Default rule to build and run the executable
$(TARGET):$(OBJS)
	$(CXX) $(CXXFLAGS) $(THREAD_FLAGS) -o $(TARGET) $(OBJS) $(LIBS) 

setcaps:
	$(SUDO_SET_CAPS)

run: $(TARGET) setcaps
	sudo ./$(TARGET)

# Clean rule to remove generated files
.PHONY: clean
clean:
	\rm -f $(TARGET) $(OBJS)
