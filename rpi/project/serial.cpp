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

Serial::Serial()
{
    serial_fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0)
        throw std::runtime_error("Failed to open serial port. Check if it is used by another device. " + std::string(strerror(errno)));

    struct termios config;

    /* Get the current options for the port and set the baudrates to 115200 */
    tcgetattr(serial_fd, &config);
    cfsetispeed(&config, B115200);
    cfsetospeed(&config, B115200);

    /* No parity (8N1) */
    config.c_cflag &= ~PARENB;
    config.c_cflag &= ~CSTOPB;
    config.c_cflag &= ~CSIZE;
    config.c_cflag |= CS8;

    /* Hardware flow control disabled */
    /* Receiver enabled and local mode set */
    config.c_cflag &= ~CRTSCTS;
    config.c_cflag |= CREAD | CLOCAL;
    /* Software control disabled */
    /* Raw input (non-canonical), with no output processing */
    config.c_iflag &= ~(IXON | IXOFF | IXANY);
    config.c_iflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);
    config.c_oflag &= ~OPOST;

    config.c_cc[VMIN] = 10;
    config.c_cc[VTIME] = 0;

    if (tcsetattr(serial_fd, TCSANOW, &config) < 0)
    {
        throw std::runtime_error("Unable to set serial configuration.\n");
    }
    tcflush(serial_fd, TCIFLUSH);
}

int Serial::getSerial()
{
    return serial_fd;
}

Serial::~Serial()
{
    close(serial_fd);
}

void Serial::sendjson(const struct can_frame received)
{
    json j;
    std::stringstream cc;
    cc << std::hex << std::setw(3) << std::showbase << received.can_id;
    j["can_id"] = cc.str();
    j["dlc"] = received.can_dlc;
    std::vector<std::string> payload_hex;
    for (int i = 0; i < received.can_dlc; ++i)
    {
        std::stringstream byte_ss;
        byte_ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(received.data[i]);
        payload_hex.push_back(byte_ss.str());
    }

    /* Joining data */
    std::stringstream payload_ss;
    payload_ss << "[";
    for (size_t i = 0; i < payload_hex.size(); ++i)
    {
        if (i != 0)
        {
            payload_ss << ",";
        }
        payload_ss << payload_hex[i];
    }
    payload_ss << "]";
    j["payload"] = payload_ss.str();
    std::string send_string = j.dump();
    serialsend(send_string);
}

void Serial::serialsend(const std::string message)
{
    const char *ch_message = message.c_str();
    int nbytes = write(serial_fd, ch_message, strlen(ch_message));
    if (nbytes < 0)
    {
        throw std::runtime_error("No bytes written to file. " + std::string(strerror(errno)));
    }
}

void Serial::serialreceive(json &j)
{
    std::cout << "SERIAL" << std::endl;
    char buf[BUFFER_SIZE];
    int buf_pos = 0;
    int bytes_read = 0;
    int json_started = 0;
    while (1)
    {
        bytes_read = read(serial_fd, &buf[buf_pos], 1);
        if (bytes_read > 0)
        {
            if (buf[buf_pos] == START_MARKER)
            {
                json_started = 1;
                buf_pos = 0;
            }

            if (json_started)
            {
                buf_pos += bytes_read;
            }

            if (buf[buf_pos - 1] == END_MARKER)
            {
                buf[buf_pos] = '\0';
                try
                {
                    std::string jsonString(buf);
                    j = json::parse(jsonString);
                    { /* Lock mutex */
                        std::unique_lock<std::mutex> lock(m);
                        /* Set global variable */
                        data_ready = true;
                    }
                    break;
                }
                catch (json::parse_error &e)
                {
                    std::cout << "Parse error: " << e.what() << std::endl;
                }

                json_started = 0;
                buf_pos = 0;
                /* Initialize integer array with zeros */
                memset((void *)buf, '0', sizeof(buf));
            }
        }
    }
}

void errorlog(const std::string &error_desc, const struct can_frame &frame)
{
    std::ofstream dataout;

    /* Get current time as time_t */
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    /* Format the time */
    std::tm tm = *std::localtime(&now_time);

    /* Open file in append mode */
    dataout.open("error.log", std::ios::app);
    if (!dataout)
    {
        std::cout << "Error: file couldn't be opened!" << std::endl;
        return;
    }
    std::cout << std::endl;
    dataout << "(" << std::put_time(&tm, "%F %T") << "): " << error_desc << std::endl;
    dataout << std::left << std::setw(15) << "interface:"
            << std::setw(10) << "can0" << std::setw(15) << "CAN ID:"
            << std::setw(15) << std::hex << std::showbase << frame.can_id << std::endl;
}
