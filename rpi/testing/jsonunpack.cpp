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

// Add new function for unpackWriteReq and unpackReadReq
std::vector<int> parsePayload(const std::string& payload) {
  std::vector<int> result;
  std::istringstream stream(payload.substr(1, payload.length() - 2)); 
  std::string item;

    while (std::getline(stream, item, ',')) { 
        result.push_back(std::stoi(item, nullptr, 16));
    }

    return result;
}

void displayFrame(const struct can_frame &packedFrame)
{
   std::cout << std::left << std::setw(15) << "interface:"
              << std::setw(10) << "can0" << std::setw(15) << "CAN ID:"
              << std::setw(10) << std::hex << std::showbase << packedFrame.can_id
              << std::setw(20) << "data length:"
              << std::setw(5) << std::dec << (int)packedFrame.can_dlc << "data: ";

    for (int k = 0; k < packedFrame.can_dlc; k++)
    {
        std::cout << std::hex << std::showbase << std::setw(2) << (int)packedFrame.data[k] << " ";
    }	
	std::cout << std::endl;
}

int main()
{
	json jsonReq = json::parse(R"({
        "can_id": "0x123",
        "dlc": 3, 
        "payload": "[0x11, 0x22, 0x33]"
    })");
	json jsonReq2 = json::parse(R"({
	"can_id":"[0x123, 0x200]",
	"can_mask" : "[0x70, 0x700]"
    })");

    // write request test
    can_frame packedFrame;
    packedFrame.can_id = std::stoi(jsonReq["can_id"].get<std::string>(), nullptr, 16);
    packedFrame.can_dlc = jsonReq["dlc"];
    auto payload = jsonReq["payload"].get<std::string>();
    auto parsedPayload = parsePayload(payload);
    std::copy(parsedPayload.begin(), parsedPayload.begin() + packedFrame.can_dlc, packedFrame.data);
    displayFrame(packedFrame);

    // read w filter request test
    auto idString = jsonReq2["can_id"].get<std::string>(), maskString = jsonReq2["can_mask"].get<std::string>();
    auto parsedId = parsePayload(idString), parsedMask = parsePayload(maskString);
    std::vector<std::pair<int, int>> filterPair;
    for(size_t i = 0; i < parsedId.size(); i++)
    {
	filterPair.emplace_back(parsedId[i], parsedMask[i]);
    } 
    for (const auto& pair : filterPair) {
    std::cout << "(" << pair.first << ", " << pair.second << ")" << std::endl;
}

    

    return 0;
}