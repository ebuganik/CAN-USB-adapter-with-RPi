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
#include <wiringPi.h>

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
     * @brief Constructor for the CANHandler class.
     *
     * This constructor initializes the CAN socket communication. It creates a socket, binds it to the specified interface, and sets up the necessary socket options.
     *
     * @param[in] ifname A constant character pointer to the name of the CAN interface (e.g 'can0' or 'can1').
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
     * @param[in] other Other CANHandler object.
     */
    CANHandler(CANHandler &&other);

    /**
     * @brief Move assignment operator.
     * @param[in] other Other CANHandler object.
     * @return Reference to this object.
     */
    CANHandler &operator=(CANHandler &&other) noexcept;

    /**
     * @brief Sends a CAN frame periodically based on a specified cycle time.
     *
     * This function sends a CAN frame at regular intervals defined by the cycle time.
     * It checks the CAN state and handles errors such as BUS OFF or ERROR WARNING states.
     * The function runs in a loop until the `isRunning` flag is set to false.
     *
     * @param[in] frame A constant reference to the CAN frame to be sent.
     * @param[in] cycle A pointer to an integer specifying the cycle time in milliseconds.
     */
    void canSendPeriod(const struct can_frame &frame, int *cycle);

    /**
     * @brief Sends a CAN frame once.
     *
     * This function sends a CAN frame through the CAN interface. It checks the CAN state and handles errors such as BUS OFF or ERROR WARNING states.
     *
     * @param[in] frame A constant reference to the CAN frame to be sent.
     */
    void canSend(const struct can_frame &frame);

    /**
     * @brief Parse frame payload from string.
     * @param[in] payload Payload string.
     * @return Vector of parsed integers.
     */
    std::vector<int> parsePayload(const std::string &payload);

    /**
     * @brief Function to unpack frame parameters from a serial write request.
     * @param[in] request Serial request as JSON.
     * @return Packed CAN frame.
     */
    struct can_frame unpackWriteReq(const json &request);

    /**
     * @brief Function to unpack desired filter can_ids and can_masks to enable filtering read.
     * @param[in] request Serial request as JSON.
     */
    void unpackFilterReq(const json &request);

    /**
     * @brief Reads a CAN frame from the CAN bus.
     *
     * This function reads a CAN frame from the CAN bus, handling timeouts and error states.
     * It sets a timeout for reading, analyzes the frame, and logs relevant information.
     *
     * @return Returns 1 on successful read, -1 on error or timeout.
     */
    int canRead();

    /**
     * @brief Enable CAN filters.
     * @param[in] filterPair Vector of filter pairs.
     */
    void canFilterEnable(std::vector<std::pair<int, int>> &filterPair);

    /**
     * @brief Enable error filtering.
     */
    void errorFilter();

    /**
     * @brief Get control error description.
     * @param[in] ctrlError Control error code (passing received/read frame's payload byte 1).
     * @param[in] errorMessage Error message string.
     */
    void getCtrlErrorDesc(unsigned char ctrlError, std::string &errorMessage);

    /**
     * @brief Get protocol error type description.
     * @param[in] protError Protocol error code (passing received/read frame's payload byte 2).
     * @param[in] errorMessage Error message string.
     */
    void getProtErrorTypeDesc(unsigned char protError, std::string &errorMessage);

    /**
     * @brief Get protocol error location description.
     * @param[in] protError Protocol error code (passing received/read frame's payload byte 3)
     * @param[in] errorMessage Error message string.
     */
    void getProtErrorLocDesc(unsigned char protError, std::string &errorMessage);

    /**
     * @brief Get transceiver status description.
     * @param[in] statusError Status error code (passing received frame's payload byte 4).
     * @param[in] errorMessage Error message string.
     */
    void getTransceiverStatus(unsigned char statusError, std::string &errorMessage);

    /**
     * @brief Analyzes a CAN frame and logs relevant information.
     *
     * This function inspects the CAN frame's ID and data to determine its type and logs appropriate messages.
     * It handles various CAN frame types including Remote Transmission Request (RTR), Extended Format (EFF), and error frames.
     *
     * @param[in] frame A constant reference to the CAN frame to be analyzed.
     */
    void frameAnalyzer(const struct can_frame &frame);

    /**
     * @brief Processes a CAN request based on the provided JSON object.
     *
     * This function handles both read and write requests for CAN communication.
     * It initializes the CAN interface by every new request, processes the request and performs the necessary actions.
     *
     * @param[in] serialRequest A reference to a JSON object containing the request details.
     * @param[in,out] socket A reference to a CANHandler object used for CAN operations.
     * @param[out] sendFrame A reference to a CAN frame structure where the data to be sent will be stored.
     * @param[out] cycle A pointer to an integer where the cycle time in milliseconds will be stored, if provided.
     */
    void processRequest(json &serialRequest, CANHandler &socket, struct can_frame &sendFrame, int *cycle);

    /**
     * @brief Blink LED function.
     * @param[in] led GPIO pin number.
     * @param[in] time Blink duration in milliseconds.
     */
    void blinkLed(int led, int time);
};

/**
 * @brief Initialize CAN interface.
 * @param[in] bitrate CAN bitrate.
 * @return Status of the initialization.
 */
int initCAN(int bitrate);

#endif /* CANHANDLER_H_ */
