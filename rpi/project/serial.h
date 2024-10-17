/**
 * @file serial.h
 * @brief Header file for serial communication handling.
 * @author Emanuela Buganik
 */

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
#include <cstdlib>
#include <sys/syslog.h>

#define START_MARKER '{' /**< Marker indicating the start of a JSON string. */
#define END_MARKER '}'   /**< Marker indicating the end of a JSON string. */
#define BUFFER_SIZE 200  /**< Buffer size for reading data from the serial port. */

using namespace std;
using json = nlohmann::json;

/**
 * @brief Mutex for thread synchronization.
 */
extern std::mutex m;

/**
 * @brief Condition variable for thread synchronization.
 */
extern std::condition_variable cv;

/**
 * @brief Global variable to track if new request is received or not.
 */
extern std::atomic<bool> dataReady;

/**
 * @brief Global variable to track if main thread is running or not.
 */
extern std::atomic<bool> isRunning;

/**
 * @brief CAN interface name global variable that can be changed.
 */
extern const char *interfaceName;

/**
 * @brief Enum class for status codes.
 */
enum class StatusCode
{
    NODE_STATUS = 500,       /**< Node status code       */
    OPERATION_ERROR = 400,   /**< Operation error code   */
    OPERATION_SUCCESS = 200, /**< Operation success code */
    TIMEOUT = 408            /**< Timeout code           */
};

/**
 * @brief Class to handle serial communication.
 */
class Serial
{
    int m_serialfd;          /**< File descriptor for the serial port */
    struct termios m_config; /**< Configuration for the serial port */

public:
    /**
     * @brief Constructor of a Serial class object.
     *
     * This constructor initializes the serial communication parameters, opens the serial port,
     * and sets the necessary configurations such as baudrate, parity, and flow control.
     */
    Serial();

    /**
     * @brief Destructor of Serial class object.
     */
    ~Serial();

    /**
     * @brief Sends status message (read frame or status on success) back as a JSON string over the serial port.
     * @param[in] statusMessage The status message to be sent.
     */
    void serialSend(const std::string statusMessage);

    /**
     * @brief Sends read frame over the serial port.
     * @param[in] receivedFrame The CAN frame to be sent.
     */
    void sendReadFrame(const struct can_frame &receivedFrame);

    /**
     * @brief Sends a status message with a status code over the serial port.
     * @param[in] code The status code.
     * @param[in] message The status message on success.
     */
    void sendStatusMessage(const StatusCode &code, const std::string &message);

    /**
     * @brief Receives and processes serial data.
     *
     * This function reads data from the serial port, detects messages as JSON strings, and parses them.
     * It runs in a loop until the `isRunning` flag is set to false.
     *
     * @param[in,out] serialRequest A reference to a JSON object where the parsed data will be stored.
     */
    void serialReceive(json &serialRequest);
};

#endif /* SERIAL_H_ */
