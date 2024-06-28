#include "interface.h"
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>

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

        while (1)
        {
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
            
            // std::string s = serial.serialreceive();
            json j = serial.serialreceive();
            
            if (j["method"] == "read")
            {
                std::cout << "Read function" << std::endl;
                // TODO: infinite loop to read from PCAN, ask if you want masked id

                // struct can_frame received = socket.canread();
                // socket.frameAnalyzer(received);
                // std::cout << std::left << std::setw(15) << "interface:"
                //           << std::setw(10) << "can0"
                //           << std::setw(15) << "CAN ID:"
                //           << std::setw(10) << std::hex << std::showbase << received.can_id
                //           << std::setw(20) << std::dec << "data length:"
                //           << std::setw(5) << (int)received.can_dlc
                //           << "data: ";

                // for (int i = 0; i < received.can_dlc; ++i)
                // {
                //     std::cout << std::hex << std::setw(2) << (int)received.data[i] << " ";
                // }

                // std::cout << std::dec << std::endl;
                //     serial.sendJSON(received);
                // }
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
