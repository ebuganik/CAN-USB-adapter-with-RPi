#ifndef SERIAL_H_
#define SERIAL_H_

#include <termios.h>
#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "../ext_lib/json.hpp" 
#include <future>
#include <atomic>
#include <csignal>

using namespace std;
using json = nlohmann::json;

/* Thread sync */
extern std::mutex m;
extern std::condition_variable cv;
/* Global variable to track if new request is received or not */
extern std::atomic<bool> dataReady;
/* Global variable to track if CTRL+C is pressed */
extern std::mutex c;
extern std::atomic<bool> isRunning;
/* Termios class to handle serial communicaton */
class Serial
{
    int m_serialfd;
    struct termios m_config;

public:
    Serial();
    ~Serial();
    int getSerial() const;
    void serialSend(const std::string statusMessage);
    void sendJson(const struct can_frame &receivedFrame);
    void serialReceive(json &serialRequest);
};


#endif /* SERIAL_H_ */