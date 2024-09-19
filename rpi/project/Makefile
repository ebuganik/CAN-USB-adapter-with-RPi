TARGET = main
CXX = g++
LIBS = -lsocketcan
CXXFLAGS = -Wall -g 
THREAD_FLAGS = -pthread
SRCS = main.cpp socketcan.cpp serial.cpp
OBJS = $(SRCS:.cpp=.o)

# Set capabilities
SUDO_SET_CAPS = sudo setcap cap_net_raw,cap_net_admin+ep ${TARGET}

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
