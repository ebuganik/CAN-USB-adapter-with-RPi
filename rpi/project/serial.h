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
extern std::atomic<bool> isRunning;

enum class StatusCode {
    NODE_STATUS = 500, 
    OPERATION_ERROR = 400,
    OPERATION_SUCCESS = 200,
    TIMEOUT = 408 
};
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
    void sendReadFrame(const struct can_frame &receivedFrame);
    void sendStatusMessage(const StatusCode &code, const std::string &message);
    void serialReceive(json &serialRequest);
};


#endif /* SERIAL_H_ */