#include "serial.h"
#include "canhandler.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <cstdlib>
#include <thread>
#include <atomic>
#include <sys/syslog.h>

std::atomic<bool> isRunning(true);
void sigintHandler(int signal)
{
    std::cout << "\nSIGINT received. Initiating shutdown..." << std::endl;
    std::unique_lock<std::mutex> lock(m);
    isRunning = false;
    cv.notify_all();
}

using namespace std;
int main()
{
    std::signal(SIGINT, sigintHandler);
    wiringPiSetupGpio();
    pinMode(17, OUTPUT);
    Serial serial;
    initCAN(200000);
    CANHandler socket;
    struct can_frame sendFrame = {0};
    int cycleTime = 0;
    json serialRequest;
    std::thread canThread(&CANHandler::canSendPeriod, &socket, std::ref(sendFrame), &cycleTime);
    while (isRunning)
    {
        serial.serialReceive(serialRequest);
        socket.processRequest(serialRequest, socket, sendFrame, &cycleTime);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    sleep(1);
    canThread.join();
    std::cout << "Program safely terminated!" << std::endl;
    return 0;
}