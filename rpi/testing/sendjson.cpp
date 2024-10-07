#include <iostream>
#include <iomanip>
#include <cstdlib>
#include "./ext_lib/json.hpp"
#include <string>
#include <vector>
#include <cstring>
#include <string.h>
#include <sstream> 
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>
#include <stdexcept>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/netlink.h>

using json = nlohmann::json;
using namespace nlohmann::literals;
using namespace std;

int main()

{  /* Testing frame */
    struct can_frame receivedFrame;
    receivedFrame.can_id = 0x123;
    receivedFrame.can_dlc = 3;
    receivedFrame.data[0] = 0x11;
    receivedFrame.data[1] = 0x22;
    receivedFrame.data[2] = 0x33;
  /* Packing frame into a JSON string */
    json jsonRequest2;
    char buffer[20];	
    sprintf(buffer, "0x%X", receivedFrame.can_id);
    jsonRequest2["can_id"] = buffer;
    jsonRequest2["dlc"] = receivedFrame.can_dlc;
    std::ostringstream payloadStream;
    payloadStream << "[";
    for (int i = 0; i < receivedFrame.can_dlc; i++) {
        payloadStream << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(receivedFrame.data[i]);
        if (i < receivedFrame.can_dlc - 1) {
            payloadStream << ", ";
        }
    }
    payloadStream << "]";
    jsonRequest2["payload"] = payloadStream.str();
    std::cout << jsonRequest2.dump() << std::endl;

    return 0;
}