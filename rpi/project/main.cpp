#include "interface.h"
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>

using namespace std;
int main()
{

    try
    {

        Serial serial;
        while (1)
        {
            // Receive JSON from serial, extract bitrate or interface name if necessary and open socket, set it up and its bitrate
            json j = serial.serialreceive();
            SocketCAN socket(j["bitrate"]);
            // Activate error filter to get error descriptions in case error frame is being received
            socket.errorFilter();

            if (j["method"] == "write")
            {
                std::cout << "----------Write function detected----------" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                socket.cansend(to_send);
            }
            else if (j["method"] == "read")
            {
                // Check if can id shall be masked or not - add logic
                std::cout <<"----------Read function detected----------" << std::endl;
                if(j["id"] != "None")
                {
                    socket.canfilterEnable();
                }
                struct can_frame received = socket.canread();
                socket.frameAnalyzer(received);
                serial.sendjson(received);
                std::cout << std::left << std::setw(15) << "interface:"
                          << std::setw(10) << "can0"
                          << std::setw(15) << "CAN ID:"
                          << std::setw(10) << std::hex << std::showbase << received.can_id
                          << std::setw(20) << std::dec << "data length:"
                          << std::setw(5) << (int)received.can_dlc
                          << "data: ";

                for (int i = 0; i < received.can_dlc; ++i)
                {
                    std::cout << std::hex << std::setw(2) << (int)received.data[i] << " ";
                }
                std::cout << std::endl;
            }
            else
            {
                std::cout << "----------Invalid request----------" << std::endl;
            }
        }
        sleep(3);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }

    return 0;
}
