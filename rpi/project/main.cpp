#include "serial.h"
#include "canhandler.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <thread>
#include <atomic>
#include <sys/syslog.h>

std::atomic<bool> isRunning(true);
const char *interfaceName;

void sigintHandler(int signal)
{
    syslog(LOG_DEBUG, "SIGINT received. Initiating shutdown");
    std::unique_lock<std::mutex> lock(m);
    isRunning = false;
    cv.notify_all();
}

using namespace std;

int main(int argc, char *argv[])
{
    syslog(LOG_INFO, "Starting C++ Application on Raspberry Pi");
    std::signal(SIGINT, sigintHandler);
    syslog(LOG_DEBUG, "Initializing RPI GPIO setup");
    wiringPiSetupGpio();
    pinMode(17, OUTPUT);
    Serial serial;
    if (argc > 1) { interfaceName = argv[1];}
    initCAN(200000);
    CANHandler socket(interfaceName);
    struct can_frame sendFrame = {0};
    int cycleTime = 0;
    json serialRequest;
    syslog(LOG_DEBUG, "Creating CAN thread to perform periodic writing to CAN bus.");
    std::thread canThread(&CANHandler::canSendPeriod, &socket, std::ref(sendFrame), &cycleTime);
    while (isRunning)
    {
        serial.serialReceive(serialRequest);
        socket.processRequest(serialRequest, socket, sendFrame, &cycleTime);
        syslog(LOG_INFO, "Thread sleeping time. Read new request or terminate application.");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    sleep(1);
    canThread.join();
    syslog(LOG_DEBUG, "Application terminated safely.");
    return 0;
}