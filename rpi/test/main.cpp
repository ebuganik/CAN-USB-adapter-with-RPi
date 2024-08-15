#include "interface.h"
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <iomanip>
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
        SocketCAN socket("can0", 115200);
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
            /* Read from serial*/
           // json j = serial.serialreceive();
           // std::cout << receive.dump()<<std::endl;
           // This is possible
            json j = {{"method","read"}};
            if (j["method"] == "write")
            {
                std::cout << "Write function detected" << std::endl;
                // cansend and sendJSON functions to be called
            }
            else
            {
                std::cout <<"Read function detected" << std::endl;
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
                // TODO: sendJSON function to be called

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
