#include <iostream>
#include <iomanip>
#include <cstdlib>
#include "../ext_lib/json.hpp"
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
#include <string_view>

using namespace std;
using json = nlohmann::json;
using namespace nlohmann::literals;


int main()

{  char sbuf;
   // Testing stored JSON data, raw string literal sent via serial, to read and write from CAN bus
   char write_test[100] = R"({"method":"write","bitrate":250000,"can_id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})";
   char read_test[150] = R"({"method":"read", "bitrate":250000, "can_id":"[0x123, 0x222]", "can_mask":"[0x7FF, 0x700]"})";

  // Convert to string and parse 
   std::string parse_write = R"({"method":"write","bitrate":250000,"id":"0x123","dlc":4,"data":"[0x11,0x22,0x33,0x44]"})";
   std::string parse_read = R"({"method":"read", "bitrate":250000, "can_id":"[0x123, 0x222]", "can_mask":"[0x7FF, 0x700]"})";
   json json_write = json::parse(parse_write);
   json json_read = json::parse(parse_read);
   json result = json::parse(parse_read);
   std::cout << std::boolalpha << result.contains("method")<< std::endl;  // works

   // Extraction
   if (result["method"] == "write")
   {
    std::string method = result["method"];
    int bitrate = result["bitrate"];
    std::string id = result["id"];
    int value = std::stoi(id, nullptr, 16);
    int dlc = result["dlc"];
    std::string dataString = result["data"];

   // Data string saved as vector array of ints
    std::vector<int> data;
    std::string cleanData = dataString.substr(1, dataString.length() - 2); 
    std::stringstream ss(cleanData);
    std::string item;
    int conv_val;
    while (std::getline(ss, item, ',')) {
      std::stringstream(item) >> std::hex >> conv_val;
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
    for (int i = 0; i < frame.can_dlc; i++)
      {

            frame.data[i] = data[i];
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
          payload_ss << payload_hex[i];
      }
      payload_ss << "]";
      j["data"] = payload_ss.str();
      std::string send_string = j.dump();
      std::cout << send_string << std::endl;
   }
   else if(result["method"] == "read")
   {
    std::string method = result["method"];
    int bitrate = result["bitrate"];
    std::vector<canid_t> canids;
    std::vector<canid_t> canmasks;
    if(result["can_id"] != "[None]")
    {
      std::string can_id = result["can_id"];
      std::string can_mask = result["can_mask"];
      std::cout << can_id << std::endl;
      std::cout << can_mask << std::endl;
      std::string cln1 = can_id.substr(1, can_id.length() - 2); 
      std::string cln2 = can_mask.substr(1, can_mask.length() - 2);
      std::stringstream ss1(cln1);
      std::stringstream ss2(cln2);
      std::string item1, item2;
      int conv_val1, conv_val2;
      while (std::getline(ss1, item1, ',') && std::getline(ss2, item2, ',')) {
        std::stringstream(item1) >> std::hex >> conv_val1;
        std::stringstream(item2) >> std::hex >> conv_val2;
        canids.push_back(conv_val1);
        canmasks.push_back(conv_val2);
         }
      std::cout << "Method: " << method << std::endl;
      std::cout << "Bitrate: " << std::dec <<bitrate << std::endl;
      std::cout << "CAN IDs: ";
      for (const auto& val : canids) {
          std::cout << std::hex << val << " " ;}
      std::cout << "CAN masks: ";
      for (const auto& val : canmasks) {
          std::cout << std::hex << val << " " ;
      }

      std::vector<std::pair<canid_t, canid_t>> can_filters;
      for (size_t i = 0; i < canids.size(); ++i)
      {
        can_filters.emplace_back(canids[i], canmasks[i]);
      }

      struct can_filter rfilter[can_filters.size()];
      for (size_t i = 0; i < can_filters.size(); ++i)
      {
        rfilter[i].can_id = can_filters[i].first;
        rfilter[i].can_mask = can_filters[i].second;
      }
    } 
  }  
    return 0;
}