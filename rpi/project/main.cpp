#include "serial.h"
#include "canhandler.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <cstdlib>
#include <thread>
#include <atomic>

using namespace std;
bool cycleTimeRec;

int main()
{
    // TODO: Minimize while loop
    Serial serial;
    initCAN(200000);
    CANHandler socket;
    struct can_frame sendFrame = {0};
    int cycleTime = 0;
    json serialRequest;
    std::thread canThread(&CANHandler::canSendPeriod, &socket, std::ref(sendFrame), &cycleTime);
    while (1)
    {
        serial.serialReceive(serialRequest);
        if (serialRequest["method"] == "write")
        {
            std::cout << "----------Write function detected----------" << std::endl;
            if (serialRequest.contains("cycle_ms"))
            {
                cycleTimeRec = true;
                cycleTime = serialRequest["cycle_ms"];
            }
            else
            {
                cycleTimeRec = false;
            }

            sendFrame = socket.unpackWriteReq(serialRequest);

            if (cycleTimeRec)
            {
                std::cout << "Cycle time in ms: " << std::dec << cycleTime << std::endl;
                std::unique_lock<std::mutex> lock(m);
                initCAN(serialRequest["bitrate"]);
                CANHandler s;
                socket = move(s);
                cv.notify_one();
            }
            else
            {
                std::cout << "No cycle time added." << std::endl;
                initCAN(serialRequest["bitrate"]);
                CANHandler writeOp;
                writeOp.canSend(sendFrame);
            }
        }
        else if (serialRequest["method"] == "read")
        {
            {
                cycleTimeRec = false;
            }
            std::cout << "----------Read function detected----------" << std::endl;
            initCAN(serialRequest["bitrate"]);
            CANHandler readOp;
            if (serialRequest.contains("can_id") && serialRequest.contains("can_mask"))
            {
                readOp.unpackWriteReq(serialRequest);
                // socket.unpackFilterReq(serialRequest);
            }
            readOp.errorFilter();
            readOp.canRead();
        }
        else
        {
            std::cout << "----------Invalid request-----------" << std::endl;
        }
    }
    return 0;
}