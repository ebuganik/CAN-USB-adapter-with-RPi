/**
 * @file CANHandler.h
 * @brief Header file for CANHandler class to handle CAN bus communication.
 * @author Emanuela Buganik
 */

#ifndef CANHANDLER_H_
#define CANHANDLER_H_

#include <linux/can.h>
#include <linux/can/error.h>
#include <linux/can/netlink.h>
#include <net/if.h>
#include <vector>
#include <utility>
#include "serial.h"
#include "../ext_lib/WiringPi/wiringPi/wiringPi.h"

/**
 * @brief Flag to track if write request with cycle_ms parameter is received.
 */
extern std::atomic<bool> cycleTimeRec;

/**
 * @class CANHandler
 * @brief Class to handle CAN bus communication.
 */
class CANHandler
{
private:
    int m_socketfd;             /**< File descriptor to read and write. */
    int m_state;                /**< State of the CAN handler. */
    const char *m_ifname;       /**< Interface name. */
    struct ifreq m_ifr;         /**< Interface request structure. */
    struct sockaddr_can m_addr; /**< CAN address structure. */

public:
    /**
     * @brief Constructor of a CANHandler object.
     * @param ifname Interface name (e.g "can0" or "can1").
     */
    CANHandler(const char *ifname);

    /**
     * @brief Destructor of a CANHandler object.
     */
    ~CANHandler();

    /**
     * @brief Deleted copy constructor.
     */
    CANHandler(const CANHandler &) = delete;

    /**
     * @brief Deleted copy assignment operator.
     */
    CANHandler &operator=(const CANHandler &) = delete;

    /**
     * @brief Move constructor.
     * @param other Other CANHandler object.
     */
    CANHandler(CANHandler &&other);

    /**
     * @brief Move assignment operator.
     * @param other Other CANHandler object.
     * @return Reference to this object.
     */
    CANHandler &operator=(CANHandler &&other) noexcept;

    /**
     * @brief Function to send CAN frame periodically.
     * @param frame CAN frame to send.
     * @param cycle Cycle time in milliseconds.
     */
    void canSendPeriod(const struct can_frame &frame, int *cycle);

    /**
     * @brief Function to send CAN frame once.
     * @param frame CAN frame to send.
     */
    void canSend(const struct can_frame &frame);

    /**
     * @brief Parse frame payload from string.
     * @param payload Payload string.
     * @return Vector of parsed integers.
     */
    std::vector<int> parsePayload(const std::string &payload);

    /**
     * @brief Function to unpack frame parameters from a serial write request.
     * @param request Serial request as JSON.
     * @return Packed CAN frame.
     */
    struct can_frame unpackWriteReq(const json &request);

    /**
     * @brief Function to unpack desired filter can_ids and can_masks to enable filtering read.
     * @param request Serial request as JSON.
     */
    void unpackFilterReq(const json &request);

    /**
     * @brief Read CAN frame from CAN bus.
     * @return Status of the read operation.
     */
    int canRead();

    /**
     * @brief Enable CAN filters.
     * @param filterPair Vector of filter pairs.
     */
    void canFilterEnable(std::vector<std::pair<int, int>> &filterPair);

    /**
     * @brief Enable error filtering.
     */
    void errorFilter();

    /**
     * @brief Get control error description.
     * @param ctrlError Control error code (passing received/read frame's payload byte 1).
     * @param errorMessage Error message string.
     */
    void getCtrlErrorDesc(unsigned char ctrlError, std::string &errorMessage);

    /**
     * @brief Get protocol error type description.
     * @param protError Protocol error code (passing received/read frame's payload byte 2).
     * @param errorMessage Error message string.
     */
    void getProtErrorTypeDesc(unsigned char protError, std::string &errorMessage);

    /**
     * @brief Get protocol error location description.
     * @param protError Protocol error code (passing received/read frame's payload byte 3)
     * @param errorMessage Error message string.
     */
    void getProtErrorLocDesc(unsigned char protError, std::string &errorMessage);

    /**
     * @brief Get transceiver status description.
     * @param statusError Status error code (passing received frame's payload byte 4).
     * @param errorMessage Error message string.
     */
    void getTransceiverStatus(unsigned char statusError, std::string &errorMessage);

    /**
     * @brief Analyze CAN frame.
     * @param frame CAN frame to analyze.
     */
    void frameAnalyzer(const struct can_frame &frame);

    /**
     * @brief Process serial request function.
     * @param serialRequest Serial request as JSON.
     * @param socket CANHandler socket.
     * @param sendFrame CAN frame to be sent.
     * @param cycle Cycle time in milliseconds.
     */
    void processRequest(json &serialRequest, CANHandler &socket, struct can_frame &sendFrame, int *cycle);

    /**
     * @brief Blink LED function.
     * @param led GPIO pin number.
     * @param time Blink duration in milliseconds.
     */
    void blinkLed(int led, int time);
};

/**
 * @brief Initialize CAN interface.
 * @param bitrate CAN bitrate.
 * @return Status of the initialization.
 */
int initCAN(int bitrate);

#endif /* CANHANDLER_H_ */
