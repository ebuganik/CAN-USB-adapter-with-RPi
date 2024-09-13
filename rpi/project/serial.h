#ifndef SERIAL_H_
#define SERIAL_H_

#include <termios.h>
#include <iostream>
#include <string>
#include "../ext_lib/json.hpp" 

using json = nlohmann::json;

/* Termios class to handle serial communicaton */
static int rec_data_flag = 0;

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