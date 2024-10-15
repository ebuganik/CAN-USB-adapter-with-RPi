#ifndef CANHANDLER_H_
#define CANHANDLER_H_

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
#include "../ext_lib/WiringPi/wiringPi/wiringPi.h"

/* Flag to track if write request with cycle_ms parameter is received */
extern std::atomic<bool> cycleTimeRec;

using namespace std;
using json = nlohmann::json;

/* CANHandler class to handle CAN bus communication */
class CANHandler
{
private:
    int m_socketfd; /* fd to read and write */
    int m_state;
    const char *m_ifname;
    struct ifreq m_ifr;
    struct sockaddr_can m_addr;

public:
    CANHandler(const char *ifname);
    ~CANHandler();
    CANHandler(const CANHandler &) = delete;
    CANHandler &operator=(const CANHandler &) = delete;
    CANHandler(CANHandler &&other);
    CANHandler &operator=(CANHandler &&other) noexcept;
    void canSendPeriod(const struct can_frame &frame, int *cycle);
    void canSend(const struct can_frame &frame);
    std::vector<int> parsePayload(const std::string &payload);
    struct can_frame unpackWriteReq(const json &request);
    void unpackFilterReq(const json &request);
    int canRead();
    void canFilterEnable(std::vector<std::pair<int, int>> &filterPair);
    void canFilterDisable();
    void loopBack(int mode);
    void errorFilter();
    void displayFrame(const struct can_frame &frame);
    void getCtrlErrorDesc(unsigned char ctrlError, std::string &errorMessage);
    void getProtErrorTypeDesc(unsigned char protError, std::string &errorMessage);   /* error in CAN protocol (type)      / data[2] */
    void getProtErrorLocDesc(unsigned char protError, std::string &errorMessage);    /* error in CAN protocol (location) / data[3] */
    void getTransceiverStatus(unsigned char statusError, std::string &errorMessage); /* error status of CAN-transceiver / data[4] */
    void frameAnalyzer(const struct can_frame &frame);
    void processRequest(json &serialRequest, CANHandler &socket, struct can_frame &sendFrame, int *cycle);
    void blinkLed(int led, int time);
};

int initCAN(int bitrate);

#endif /* CANHANDLER_H_*/
