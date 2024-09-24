#ifndef SOCKETCAN_H_
#define SOCKETCAN_H_

#include <string>
#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/netlink.h>
#include <net/if.h>
#include <termios.h>
#include <vector>
#include <utility>
#include "serial.h"
#include "../ext_lib/json.hpp" 

using namespace std;
using json = nlohmann::json;

/* SocketCAN class to handle CAN bus communication */
class SocketCAN
{
private:
    int socket_fd;   /* fd to read and write */
    int socket_ctrl; /* fd to control interface status */
    int bitrate;
    const char* ifname = "can0";
    struct ifreq ifr;
    struct sockaddr_can addr;
    struct can_frame frame;

public:
    SocketCAN(int bitrate);
    ~SocketCAN();
    int get_fd();
    void set_fd(int s);
    void cansend(int* cycle); 
    struct can_frame jsonunpack(const json &j);
    int canread();
    void canfilterEnable(std::vector<std::pair<canid_t, canid_t>> &filter);
    void canfilterDisable();
    void loopback(int value);
    void errorFilter();
    void displayFrame(const struct can_frame &frame);
    std::string getCtrlErrorDesc(__u8 ctrl_error);
    std::string getProtErrorTypeDesc(__u8 prot_error);   /* error in CAN protocol (type) / data[2] */
    std::string getProtErrorLocDesc(__u8 prot_error);    /* error in CAN protocol (location) / data[3] */
    std::string getTransceiverStatus(__u8 status_error); /* error status of CAN-transceiver / data[4] */
    void frameAnalyzer(const struct can_frame &frame);
    bool isCANUp();                                      
    int initCAN(int bitrate);                            /* Function that uses libsocketcan functions to set CAN interface up */
    std::string checkState();
};

#endif /* SOCKETCAN_H_*/
