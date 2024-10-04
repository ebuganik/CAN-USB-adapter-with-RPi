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

/* Flag to track if write request with cycle_ms parameter is received */
extern bool cycleTimeRec;

/* CANHandler class to handle CAN bus communication */
class CANHandler
{
private:
    int m_socketfd;   /* fd to read and write */
    const char* m_ifname = "can0";
    struct ifreq m_ifr;
    struct sockaddr_can m_addr;

public:
    CANHandler(int bitrate);
    ~CANHandler();
    int getFd() const;
    void setFd(int t_socketfd);
    void canSendPeriod(const struct can_frame &frame, int* cycle); 
    void canSend(const struct can_frame &frame);
    struct can_frame jsonUnpack(const json &request);
    int canRead();
    void canFilterEnable(std::vector<std::pair<canid_t, canid_t>> &filterPair);
    void canFilterDisable();
    void loopBack(int mode);
    void errorFilter();
    void displayFrame(const struct can_frame &frame);
    std::string getCtrlErrorDesc(__u8 ctrlError);
    std::string getProtErrorTypeDesc(__u8 protError);   /* error in CAN protocol (type) / data[2] */
    std::string getProtErrorLocDesc(__u8 protError);    /* error in CAN protocol (location) / data[3] */
    std::string getTransceiverStatus(__u8 statusError); /* error status of CAN-transceiver / data[4] */
    void frameAnalyzer(const struct can_frame &frame);
    int initCAN(int bitrate);                            /* Function that uses libsocketcan functions to set CAN interface up */
    std::string checkState();
};

#endif /* SOCKETCAN_H_*/
