#include "serial.h"
#include "socketcan.h"
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
            /* Receive JSON from serial, extract bitrate or interface name if necessary and open socket, set it up and its bitrate */ 
            json j = serial.serialreceive();
            SocketCAN socket(j["bitrate"]);
            /* Activate error filter to get error descriptions in case error frame is being received */
            socket.errorFilter();
            if (j["method"] == "write")
            {
                std::cout << "----------Write function detected----------" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                socket.cansend(to_send);
            }
            else if (j["method"] == "read")
            {
                // TODO: Check if can id shall be masked or not - add logic
                std::cout <<"----------Read function detected----------" << std::endl;
                if(j["id"] != "None")
                {
                    // TODO: Send unpacked json data to extract mask and desired can id
                    socket.canfilterEnable();
                }
                socket.canread();
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
