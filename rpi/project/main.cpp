#include "serial.h"
#include "socketcan.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <cstdlib>
#include <thread>

using namespace std;

int main()
{

    try
    {

        Serial serial;
        while (1)
        {
            json j;
            /* Receive JSON from serial, extract bitrate or interface name if necessary and open socket, set it up and its bitrate */
            // serial.serialreceive(j);
            std::thread t(&Serial::serialreceive, &serial, std::ref(j));
            t.join();
            SocketCAN socket(j["bitrate"]);
            /* Activate error filter to get error descriptions in case error frame is being received */
            socket.errorFilter();
            if (j["method"] == "write")
            {
                std::cout << "----------Write function detected----------" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                if (j.contains("cycle_ms"))
                    socket.cansend(to_send, j["cycle_ms"]);
                else
                    socket.cansend(to_send, 0);
            }
            else if (j["method"] == "read")
            {
                std::cout << "----------Read function detected----------" << std::endl;
                /* Check whether can_id and can_mask are sent to set receive filter */
                if (j.contains("can_id") && j.contains("can_mask"))
                    socket.jsonunpack(j);
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
        if (std::string(e.what()) == "Error in initializing CAN interface!")
        {
            std::cout << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
        else
            std::cout << e.what() << std::endl;
    }

    return 0;
}
