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

{  char sbuf;
   // Testing stored JSON data, raw string literal sent via serial
   char buffer[100] = R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})";
   std::cout << buffer << std::endl;
   std::cout << sizeof(buffer) << std::endl;
   std::string jsonParse = R"({"method":"read","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})";
   json jsonString = json::parse(jsonParse);
   std::cout << jsonString << std::endl;
   std::cout <<jsonString["method"]<<std::endl;
   // Extraction check
   std::string method = jsonString["method"];
   std::cout << method << std::endl;
   int bitrate = jsonString["bitrate"];
   std::cout << bitrate << std::endl;
   std::string id = jsonString["id"];
   std::cout << "ID string:"<< id << std::endl;
   int value = std::stoi(id, nullptr, 16);
   std::cout << value << std::endl;
   std::cout << std::hex << value << std::endl;
   int dlc = jsonString["dlc"];
   std::string dataString = jsonString["data"];
   std::cout << "Data string: "<< dataString <<std::endl;
   std::cout << std::endl;
   // Data string saved as vector array of ints
   std::vector<int> data;
   std::string cleanData = dataString.substr(1, dataString.length() - 2); 
   std::stringstream ss(cleanData);
   std::string item;
   int conv_val;
   while (std::getline(ss, item, ',')) {
   	std::stringstream(item) >> std::hex >> conv_val;
	std::cout << std::showbase << conv_val << std::endl;
   	data.push_back(conv_val); }
   // Unpacked data info - prints ok
   std::cout << "Method: " << method << std::endl;
   std::cout << "Bitrate: " << std::dec <<bitrate << std::endl;
   std::cout << "ID: " << std::hex << std::showbase << value << std::endl;
   std::cout << "DLC: " << std::dec<< std::showbase <<static_cast<int>(dlc) << std::endl;
   std::cout << "Data: ";
   for (const auto& val : data) {
       std::cout << std::hex << val << " " ;}
   // Apply data to can_frame 
   struct can_frame frame;
   frame.can_id = value;
   frame.can_dlc = dlc; 
   std::cout << std::dec << std::showbase <<  (int)frame.can_dlc << std::endl;
   for (int i = 0; i < frame.can_dlc; i++)
     {

           frame.data[i] = data[i];
     }
   std::cout << std::endl;
		
   std::cout << std::left << std::setw(15) << "interface:"
                          << std::setw(10) << "can0"
                          << std::setw(15) << "CAN ID:"
                          << std::setw(10) << std::hex << std::showbase << frame.can_id
                          << std::setw(20) << "data length:"
                          << std::setw(5) <<std::dec<< std::showbase << (int)frame.can_dlc<< "data: ";

   for (int i = 0; i < frame.can_dlc; ++i)
     {
        std::cout << std::hex << std::showbase<< std::setw(2) << (int)frame.data[i] << " ";
     }
   // packing frame into JSON string
   std::cout << std::endl;
   std::cout << "JSON to be sent ..."<<std::endl;
   json j;
   j["method"] = "canread";
   std::stringstream cc;
   cc << std::hex << std::setw(3) << std::showbase << frame.can_id;
   j["id"] = cc.str();
   j["dlc"] = frame.can_dlc;
   std::vector<std::string> payload_hex;
   for (int i = 0; i < frame.can_dlc; ++i) {
        std::stringstream byte_ss;
	// std::cout << frame.data[i]<< std::endl; convert!
        byte_ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(frame.data[i]);
        payload_hex.push_back(byte_ss.str());
    }

    // Joining data 
    std::stringstream payload_ss;
    payload_ss << "[";
    for (size_t i = 0; i < payload_hex.size(); ++i) {
        if (i != 0) {
            payload_ss << ",";
        }
	// std::cout << payload_hex[i];
        payload_ss << payload_hex[i];
    }
    payload_ss << "]";
    j["data"] = payload_ss.str();
    std::string send_string = j.dump();
    std::cout << send_string << std::endl;

    return 0;
}