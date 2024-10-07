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
    CANHandler socket(200000);
    struct can_frame sendFrame = {0};
    int cycleTime = 0;
    json serialRequest;
    std::thread canThread(&CANHandler::canSendPeriod, &socket, std::ref(sendFrame), &cycleTime);
    while (1)
    {
        std::thread serialThread(&Serial::serialReceive, &serial, std::ref(serialRequest));
        serialThread.join();
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
                close(socket.getFd());
                CANHandler socket(serialRequest["bitrate"]);
                socket.setFd(-1);
                cv.notify_one();
            }
            else
            {
                std::cout << "No cycle time added." << std::endl;
                close(socket.getFd());
                CANHandler s(serialRequest["bitrate"]);
                socket.setFd(s.getFd());
                s.setFd(-1);
                socket.canSend(sendFrame);
            }
        }
        else if (serialRequest["method"] == "read")
        {
            {
                cycleTimeRec = false;
            }
            std::cout << "----------Read function detected----------" << std::endl;
            close(socket.getFd());
            CANHandler s(serialRequest["bitrate"]);
            socket.setFd(s.getFd());
            s.setFd(-1);
            if (serialRequest.contains("can_id") && serialRequest.contains("can_mask"))
            {
                socket.unpackWriteReq(serialRequest);
                // socket.unpackFilterReq(serialRequest);
            }
            socket.errorFilter();
            socket.canRead();
        }
        else
        {
            std::cout << "----------Invalid request-----------" << std::endl;
        }
    }
    return 0;
}