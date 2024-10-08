#include "serial.h"
#include <unistd.h>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <fstream>
#include <fcntl.h>
#include <linux/can/raw.h>

#define START_MARKER '{'
#define END_MARKER '}'
#define BUFFER_SIZE 200

using namespace std;
/* Constructor to initialize serial communication */

Serial::Serial()
{
    m_serialfd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_SYNC);
    if (m_serialfd < 0)
        throw std::runtime_error("Failed to open serial port. Check if it is used by another device. " + std::string(strerror(errno)));

    /* Get the current options of the port and set baudrates to 115200 */
    tcgetattr(m_serialfd, &m_config);
    cfsetispeed(&m_config, B115200);
    cfsetospeed(&m_config, B115200);

    /* No parity (8N1) */
    m_config.c_cflag &= ~PARENB;
    m_config.c_cflag &= ~CSTOPB;
    m_config.c_cflag &= ~CSIZE;
    m_config.c_cflag |= CS8;

    /* Hardware flow control disabled */
    /* Receiver enabled and local mode set */
    m_config.c_cflag &= ~CRTSCTS;
    m_config.c_cflag |= CREAD | CLOCAL;
    /* Software control disabled */
    /* Raw input (non-canonical), with no output processing */
    m_config.c_iflag &= ~(IXON | IXOFF | IXANY);
    m_config.c_iflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    m_config.c_oflag &= ~OPOST;

    m_config.c_cc[VMIN] = 10;
    m_config.c_cc[VTIME] = 0;

    if (tcsetattr(m_serialfd, TCSANOW, &m_config) < 0)
    {
        throw std::runtime_error("Unable to set serial configuration.\n");
    }
    tcflush(m_serialfd, TCIFLUSH);
}

int Serial::getSerial() const
{
    return m_serialfd;
}

Serial::~Serial()
{
    close(m_serialfd);
}

/* Pack received frames from CAN bus into JSON strings and send them via serial */
void Serial::sendJson(const struct can_frame &receivedFrame)
{
    json packedFrame;
    char buffer[20];
    sprintf(buffer, "0x%X", receivedFrame.can_id);
    packedFrame["can_id"] = buffer;
    packedFrame["dlc"] = receivedFrame.can_dlc;
    std::ostringstream payloadStream;
    payloadStream << "[";
    for (int i = 0; i < receivedFrame.can_dlc; i++)
    {
        payloadStream << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(receivedFrame.data[i]);
        if (i < receivedFrame.can_dlc - 1)
        {
            payloadStream << ", ";
        }
    }
    payloadStream << "]";
    packedFrame["payload"] = payloadStream.str();
    std::string jsonString = packedFrame.dump();
    serialSend(jsonString);
}

void Serial::serialSend(const std::string statusMessage)
{
    const char *pMessage = statusMessage.c_str();
    int nbytes = write(m_serialfd, pMessage, strlen(pMessage));
    if (nbytes < 0)
    {
        throw std::runtime_error("No bytes written to file. " + std::string(strerror(errno)));
    }
}

/* Function to read from serial port */

void Serial::serialReceive(json &serialRequest)
{
    char buf[BUFFER_SIZE];
    int bufPos = 0;
    int bytesRead = 0;
    int jsonStarted = 0;
    while (1)
    {
        bytesRead = read(m_serialfd, &buf[bufPos], 1);
        if (bytesRead > 0)
        {
            if (buf[bufPos] == START_MARKER)
            {
                jsonStarted = 1;
                bufPos = 0;
            }

            if (jsonStarted)
            {
                bufPos += bytesRead;
            }

            if (buf[bufPos - 1] == END_MARKER)
            {
                buf[bufPos] = '\0';

                // TODO: exception to error code
                try
                {
                    std::string reqString(buf);
                    serialRequest = json::parse(reqString);
                    { /* Lock mutex */
                        std::unique_lock<std::mutex> lock(m);
                        /* Set global variable */
                        dataReady = true;
                    }
                    break;
                }
                catch (json::parse_error &e)
                {
                    std::cout << "Parse error: " << e.what() << std::endl;
                }

                jsonStarted = 0;
                bufPos = 0;
                /* Initialize integer array with zeros */
                memset((void *)buf, '0', sizeof(buf));
            }
        }
    }
}
