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

using namespace std;
using json = nlohmann::json;

/* Thread sync */
extern std::mutex m;
extern std::condition_variable cv;
/* Global variable to track if new request is received or not */
extern std::atomic<bool> data_ready;
/* Termios class to handle serial communicaton */
class Serial
{
    int serial_fd;
    struct termios config;

public:
    Serial();
    ~Serial();
    int getSerial();
    void serialsend(const std::string message);
    void sendjson(const struct can_frame received);
    void serialreceive(json &j);
};

void errorlog(const std::string& error_desc, const struct can_frame &frame );

#endif /* SERIAL_H_ */