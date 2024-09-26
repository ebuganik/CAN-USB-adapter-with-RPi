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
            serial.serialreceive(j);
            // std::thread serialThread(&Serial::serialreceive, &serial, std::ref(j));
            // serialThread.join();
            SocketCAN socket(j["bitrate"]);
            socket.errorFilter();

            if (j["method"] == "write")
            {
                std::cout << "----------Write function detected----------" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                if (j.contains("cycle_ms"))
                {

                    std::thread canThread(&SocketCAN::cansend, &socket, to_send, j["cycle_ms"]);
                    canThread.join();
                    // socket.cansend(to_send, j["cycle_ms"]);
                }
                else
                {
                    socket.cansend(to_send, 0);
                }
            }
            else if (j["method"] == "read")
            {
                std::cout << "----------Read function detected----------" << std::endl;
                if (j.contains("can_id") && j.contains("can_mask"))
                {
                    socket.jsonunpack(j);
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
    // TODO
    catch (const std::exception &e)
    {
        if (std::string(e.what()) == "Error in initializing CAN interface!")
        {
            std::cout << e.what() << std::endl;
            exit(EXIT_FAILURE);
        }
        else
        {
            std::cout << e.what() << std::endl;
        }
    }

    return 0;
}