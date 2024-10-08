#include "serial.h"
#include "canhandler.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <cstdlib>
#include <thread>
#include <atomic>

std::atomic<bool> isRunning(true);
// TODO: finish handling CTRL+C
void sigintHandler(int signal) {
    std::cout << "CTRL+C pressed" << std::endl;
    isRunning.store(false);
    std::cout << std::boolalpha << isRunning << std::endl;
}

using namespace std;
int main()
{
    Serial serial;
    initCAN(200000);
    CANHandler socket;
    struct can_frame sendFrame = {0};
    int cycleTime = 0;
    json serialRequest;
    std::signal(SIGINT, sigintHandler);
    std::thread canThread(&CANHandler::canSendPeriod, &socket, std::ref(sendFrame), &cycleTime);
    while (isRunning)
    {
        serial.serialReceive(serialRequest);
        socket.processRequest(serialRequest, socket, sendFrame, &cycleTime);
    }
    std::cout << "I'm out " << std::endl;
    canThread.join();
    return 0;
}