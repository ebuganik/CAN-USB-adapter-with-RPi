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
        // /* Receiving CAN frames with enabled CAN filter enabled and to detect can error frames */
        // socket.canfilterEnable();
        // /* Receiving CAN frames with disabled CAN filter */
        // // socket.canfilter_disable();
        // /* Setting up CAN error filter */
        // socket.errorFilter();
        // /* Loopback mode */
        // // socket.loopback(1);
        SocketCAN socket("can0", 250000);
        socket.canfilterEnable();
        /* Check up if interface is up or down*/
        if (socket.isCANUp("can0"))
        {
            std::cout << " CAN0 is up." << std::endl;
        }
        else
        {
            std::cout << "CAN0 is down." << std::endl;
        }
            
        while (1)
        {
            json j = serial.serialreceive();
            if (j["method"] == "write")
            {
                std::cout << "Write function detected" << std::endl;
                struct can_frame to_send = socket.jsonunpack(j);
                socket.cansend(to_send);
            }
            else if (j["method"] == "read")
            {
                // Check if can id shall be masked or not
                std::cout <<"Read function detected" << std::endl;
                sleep(2000);
                struct can_frame received = socket.canread();
                // serial.sendJSON(received);
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

            }
            else
            {
                std::cout << "Faulty message received over serial!" << std::endl;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }

    return 0;
}
