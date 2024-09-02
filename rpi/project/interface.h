#pragma once
#include <stdio.h>
#include <string>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/netlink.h>
#include <net/if.h>
#include <termios.h>
#include "../ext_lib/json.hpp" 

using json = nlohmann::json;

/* SocketCAN class to handle CAN bus communication */
class SocketCAN
{
private:
    int socket_fd;   /* fd to read and write */
    int socket_ctrl; /* fd to control interface status */
    const char* ifname = "can0";
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_frame frame;

public:
    SocketCAN(int bitrate);
    ~SocketCAN();
    int cansend(const struct can_frame &frame); 
    struct can_frame jsonunpack(json j);
    int canread();
    void canfilterEnable();
    void canfilterDisable();
    void loopback(int value);
    void errorFilter();
    std::string getCtrlErrorDesc(__u8 ctrl_error);
    std::string getProtErrorTypeDesc(__u8 prot_error);   /* error in CAN protocol (type) / data[2] */
    std::string getProtErrorLocDesc(__u8 prot_error);    /* error in CAN protocol (location) / data[3] */
    std::string getTransceiverStatus(__u8 status_error); /* error status of CAN-transceiver / data[4] */
    void frameAnalyzer(const struct can_frame &frame);
    bool isCANUp();                                      
    int initCAN(int bitrate);                            /* Function that uses libsocketcan functions to set CAN interface up */
    std::string checkState();
};

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
    json serialreceive();
};

void errorlog(const std::string& error_desc, const struct can_frame &frame );