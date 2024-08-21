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
            SocketCAN socket("can0", j["bitrate"]);
            if(socket.isCANUp("can0"))
            std::cout << "CAN is up!" << std::endl;
            else std::cout << "CAN is down!" << std::endl;
            // Activate error filter to get error descriptions in case error frame is being received
            socket.errorFilter();

            if (j["method"] == "write")
            {
                std::cout << "----------Write function detected----------" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                if(socket.cansend(to_send)!=-1)
                    serial.serialsend("ACK: CAN frame sent successfully!\n");
                else serial.serialsend("NACK: Sending CAN frame unsuccessfull!\n");
            }
            else if (j["method"] == "read")
            {
                // Check if can id shall be masked or not
                std::cout <<"----------Read function detected----------" << std::endl;
                struct can_frame received = socket.canread();
                socket.frameAnalyzer(received);
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
                serial.sendjson(received);
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
