#include "interface.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <fcntl.h>
#include <vector>
#include <fstream>
#include <ctime>
#include <chrono>
#include <libsocketcan.h>
#include <stdio.h>
// Added to use timeout in canread function
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

#define START_MARKER '{'
#define END_MARKER '}'
#define BUFFER_SIZE 200
using namespace std;

SocketCAN::SocketCAN(int bitrate)
{

    if (initCAN(bitrate) != 0)
    {   std::cout << std::endl;
        std::cout << "can0 interface set to bitrate " << bitrate << std::endl;
    }
    else
        throw std::runtime_error("Error in initializing CAN interface!");

    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0)
        throw std::runtime_error("Error in creating socket: " + std::string(strerror(errno)));

    strcpy(ifr.ifr_name, ifname);
    ioctl(socket_fd, SIOCGIFINDEX, &ifr);

    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_family = AF_CAN;

    if (bind(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        throw std::runtime_error("Error in socket binding: " + std::string(strerror(errno)));
    }
}

SocketCAN::~SocketCAN()
{

    close(socket_fd);
    close(socket_ctrl);
}

int SocketCAN::cansend(const struct can_frame &frame)
{
    Serial inform;
    if ((checkState() == "BUS OFF STATE") || (checkState() == "BUS WARNING STATE"))
    {
        inform.serialsend("Unable to send data, bus off! Restarting interface...\n");
        can_do_restart(ifname);
        return -1;
    }
    if (write(socket_fd, &frame, sizeof(frame)) != sizeof(struct can_frame))
    {
        throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
        inform.serialsend("NACK: CAN frame sent unsuccessfully!\n");
        return -1;
    }
    else
    {
        inform.serialsend("ACK:CAN frame sent successfully!\n");
        return 1;
    }
}

struct can_frame SocketCAN::jsonunpack(json j)
{
    std::string method = j["method"];
    int bitrate = j["bitrate"];
    std::string id = j["can_id"];
    int val = stoi(id, 0, 16);
    int dlc = j["dlc"];
    std::string dataString = j["payload"];
    std::vector<int> data;
    std::string cleanData = dataString.substr(1, dataString.length() - 2);
    std::stringstream ss(cleanData);
    std::string item;
    int conv_val;

    while (std::getline(ss, item, ','))
    {
        std::stringstream(item) >> std::hex >> conv_val;
        data.push_back(conv_val);
    }

    /* Appply json parameters to can_frame struct */
    struct can_frame frame;
    frame.can_id = val;
    frame.can_dlc = dlc;
    for (int i = 0; i < frame.can_dlc; i++)
    {

        frame.data[i] = data[i];
    }
    std::cout << std::left << std::setw(15) << "interface:"
              << std::setw(10) << "can0" << std::setw(15) << "CAN ID:"
              << std::setw(10) << std::hex << std::showbase << frame.can_id
              << std::setw(20) << "data length:"
              << std::setw(5) << std::dec << std::showbase << (int)frame.can_dlc << "data: ";

    for (int i = 0; i < frame.can_dlc; ++i)
    {
        std::cout << std::hex << std::showbase << std::setw(2) << (int)frame.data[i] << " ";
    }
    std::cout << std::endl;
    return frame;
}

int SocketCAN::canread()
{
    Serial inform;
    struct can_frame receive;
    /* To set timeout in read function */
    struct timeval timeout;
    fd_set set;
    int retval;

    FD_ZERO(&set);
    FD_SET(socket_fd, &set);

    /* 10 seconds timeout */
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    retval = select(socket_fd + 1, &set, NULL, NULL, &timeout);

    if (retval == -1)
    {
        std::cout << "An error ocurred in timeout set up!" << std::endl;
        inform.serialsend("Unable to set timeout, no bytes read from CAN bus!");
        return -1;
    }
    else if (retval == 0)
    {
        std::cout << "No data within 10 seconds." << std::endl;
        inform.serialsend("CONNECTION TIMEOUT. No bytes read within 10 seconds!");
        return -1;
    }
    else
    {
        while (1)
        {
            if (read(socket_fd, &frame, sizeof(struct can_frame)))
            {

                if(checkState()== "ERROR ACTIVE STATE")
                {
                    /* In this case, it's probably SFF, RTR or EFF */
                    frameAnalyzer(frame);
                    std::cout << std::left << std::setw(15) << "interface:"
                            << std::setw(10) << "can0"
                            << std::setw(15) << "CAN ID:"
                            << std::setw(10) << std::hex << std::showbase << frame.can_id
                            << std::setw(20) << std::dec << "data length:"
                            << std::setw(5) << (int)frame.can_dlc
                            << "data: ";

                    for (int i = 0; i < frame.can_dlc; ++i)
                    {
                        std::cout << std::hex << std::setw(2) << (int)frame.data[i] << " ";
                    }
                    inform.sendjson(frame);
                    break;
                }
                /* This part checks error frames */
            else frameAnalyzer(frame); 
            }
         }
         return 1;
    }
}

// TODO: change this in case user wants to read frames with given CAN ID 
void SocketCAN::canfilterEnable()
{
    struct can_filter rfilter[2];
    rfilter[0].can_id = 0x123;
    rfilter[0].can_mask = CAN_SFF_MASK;

    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
        throw std::runtime_error("Unable to set reception filter: " + std::string(strerror(errno)));
}

void SocketCAN::canfilterDisable()
{
    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0) < 0)
        throw std::runtime_error("Unable to reset reception filter: " + std::string(strerror(errno)));
}

void SocketCAN::errorFilter()
{

    can_err_mask_t err_mask = CAN_ERR_MASK;
    if (setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0)
        throw std::runtime_error("Unable to set error filter: " + std::string(strerror(errno)));
}

void SocketCAN::loopback(int value)
{
    /* Local loopback is enabled by default and can be separately disabled for every socket
    When enabled, it is possible to receive your own messages */
    /* 0 - disabled, 1 - enabled */
    if (value != 1)
    {
        setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &value, sizeof(value));
    }
    else
    {
        int recv_own_msgs = 1; /* 0 = disabled (default), 1 = enabled */
        setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &value, sizeof(value));
        setsockopt(socket_fd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, sizeof(recv_own_msgs));
    }
}

std::string SocketCAN::getCtrlErrorDesc(__u8 ctrl_error)
{
    /* Subclasses of control error frames */

    std::string errorDesc = "";
    Serial inform;
    switch (ctrl_error)
    {
    case CAN_ERR_CRTL_UNSPEC:
        errorDesc += "unspecified";
        break;
    case CAN_ERR_CRTL_RX_OVERFLOW:
        errorDesc += "RX buf overflow";
        break;
    case CAN_ERR_CRTL_TX_OVERFLOW:
        errorDesc += "TX buf overflow";
        break;
    case CAN_ERR_CRTL_RX_WARNING:
        errorDesc += "reached warning level for RX errors";
        break;
    case CAN_ERR_CRTL_TX_WARNING:
        errorDesc += "reached warning level for TX errors";
        break;
    case CAN_ERR_CRTL_RX_PASSIVE:
        errorDesc += "reached error passive status RX";
        inform.serialsend("ERROR PASSIVE RX STATE\n");
        break;
    case CAN_ERR_CRTL_TX_PASSIVE:
        errorDesc += "reached error passive status TX";
        inform.serialsend("ERROR PASSIVE TX STATE\n");
        break;
    case CAN_ERR_CRTL_ACTIVE:
        errorDesc += "recovered to error active state";
        inform.serialsend("ERROR ACTIVE STATE\n");

        break;
    default:
        errorDesc += "unknown error state";
    }
    return errorDesc;
}

std::string SocketCAN::getProtErrorTypeDesc(__u8 prot_error)
{
    std::string errorDesc = "";
    switch (prot_error)
    {
    case CAN_ERR_PROT_UNSPEC:
        errorDesc += "unspecified";
        break;
    case CAN_ERR_PROT_BIT:
        errorDesc += "single bit error";
        break;
    case CAN_ERR_PROT_FORM:
        errorDesc += "frame format error";
        break;
    case CAN_ERR_PROT_STUFF:
        errorDesc += "bit stuffing error";
        break;
    case CAN_ERR_PROT_BIT0:
        errorDesc += "unable to send dominant bit";
        break;
    case CAN_ERR_PROT_BIT1:
        errorDesc += "unable to send recessive bit";
        break;
    case CAN_ERR_PROT_OVERLOAD:
        errorDesc += "bus overload";
        break;
    case CAN_ERR_PROT_ACTIVE:
        errorDesc += "active error announcement";
    case CAN_ERR_PROT_TX:
        errorDesc += "error occured on transmission";
        break;
    default:
        errorDesc += "unknown error state";
    }
    return errorDesc;
}

std::string SocketCAN::getProtErrorLocDesc(__u8 prot_error)
{
    std::string errorDesc = "";
    switch (prot_error)
    {
    case CAN_ERR_PROT_LOC_UNSPEC:
        errorDesc += "unspecified";
        break;
    case CAN_ERR_PROT_LOC_SOF:
        errorDesc += "start of frame";
        break;
    case CAN_ERR_PROT_LOC_ID28_21:
        errorDesc += "ID bits 28-21 (SFF 10-3)";
        break;
    case CAN_ERR_PROT_LOC_ID20_18:
        errorDesc += "ID bits 20 - 18 (SFF: 2 - 0 )";
        break;
    case CAN_ERR_PROT_LOC_SRTR:
        errorDesc += "substitute RTR (SFF: RTR)";
        break;
    case CAN_ERR_PROT_LOC_IDE:
        errorDesc += "identifier extension";
        break;
    case CAN_ERR_PROT_LOC_ID17_13:
        errorDesc += "ID bits 17-13";
        break;
    case CAN_ERR_PROT_LOC_ID12_05:
        errorDesc += "ID bits 12-5";
        break;
    case CAN_ERR_PROT_LOC_ID04_00:
        errorDesc += "ID bits 4-0";
        break;
    case CAN_ERR_PROT_LOC_RTR:
        errorDesc += "RTR";
        break;
    case CAN_ERR_PROT_LOC_RES1:
        errorDesc += "reserved bit 1";
        break;
    case CAN_ERR_PROT_LOC_RES0:
        errorDesc += "reserved bit 0";
        break;
    case CAN_ERR_PROT_LOC_DLC:
        errorDesc += "data length code";
        break;
    case CAN_ERR_PROT_LOC_DATA:
        errorDesc += "data section";
        break;
    case CAN_ERR_PROT_LOC_CRC_SEQ:
        errorDesc += "CRC sequence";
        break;
    case CAN_ERR_PROT_LOC_CRC_DEL:
        errorDesc += "CRC delimiter";
        break;
    case CAN_ERR_PROT_LOC_ACK:
        errorDesc += "ACK slot";
        break;
    case CAN_ERR_PROT_LOC_ACK_DEL:
        errorDesc += "ACK delimiter";
        break;
    case CAN_ERR_PROT_LOC_EOF:
        errorDesc += "end of frame";
        break;
    case CAN_ERR_PROT_LOC_INTERM:
        errorDesc += "intermission";
        break;
    default:
        errorDesc += "unknown error state";
    }
    return errorDesc;
}

std::string SocketCAN::getTransceiverStatus(__u8 status_error)
{
    std::string errorDesc = "";
    switch (status_error)
    {
    case CAN_ERR_TRX_UNSPEC:
        errorDesc += "unspecified";
        break;
    case CAN_ERR_TRX_CANH_NO_WIRE:
        errorDesc += "CANH no wire";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_BAT:
        errorDesc += "CANH short to BAT";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_VCC:
        errorDesc += "CANH short to VCC";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_GND:
        errorDesc += "CANH short to GND";
        break;
    case CAN_ERR_TRX_CANL_NO_WIRE:
        errorDesc += "CANL no wire";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_BAT:
        errorDesc += "CANL short to BAT";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_VCC:
        errorDesc += "CANL short to VCC";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_GND:
        errorDesc += "CANL short to GND";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_CANH:
        errorDesc += "CANL short to CANH";
        break;
    default:
        errorDesc += "unknown error state";
    }
    return errorDesc;
}

void SocketCAN::frameAnalyzer(const struct can_frame &frame)
{
    Serial inform;
    if (frame.can_id & CAN_RTR_FLAG)
    {
        std::cout << "RTR frame | ";
    }
    else if (frame.can_id & CAN_EFF_FLAG)
    {
        std::cout << "Extended Format Frame | ";
    }
    else if (frame.can_id & CAN_ERR_FLAG)
    {
        std::string errorMsg = "";

        if (frame.can_id & CAN_ERR_TX_TIMEOUT)
            errorMsg += "TX Timeout |";
        else if (frame.can_id & CAN_ERR_LOSTARB)
        {
            errorMsg += "Lost arbitration";
            if (frame.data[0] == CAN_ERR_LOSTARB_UNSPEC)
                errorMsg += "... bit unspecified";
        }
        else if (frame.can_id & CAN_ERR_CRTL)
        {
            errorMsg += "Controller problems - ";
            errorMsg += getCtrlErrorDesc(frame.data[1]);
        }
        else if (frame.can_id & CAN_ERR_PROT)
        {
            errorMsg += "Protocol violations - ";
            errorMsg += getProtErrorTypeDesc(frame.data[2]);
            errorMsg += getProtErrorLocDesc(frame.data[3]);
        }

        else if (frame.can_id & CAN_ERR_TRX)
        {
            errorMsg += "Transciever status - ";
            errorMsg += getTransceiverStatus(frame.data[4]);
        }
        else if (frame.can_id & CAN_ERR_ACK)
            errorMsg += "Received no ACK on transmission";
        else if (frame.can_id & CAN_ERR_BUSOFF)
        {
            std::cout << "BUS OFF ... Restarting can0 interface" << std::endl;
            inform.serialsend("BUS OFF STATE");
            can_do_restart(ifname);
        }
        else if (frame.can_id & CAN_ERR_BUSERROR)
            errorMsg += "Bus error (may flood!)";
        else if (frame.can_id & CAN_ERR_RESTARTED)
            errorMsg += "Controller restarted";
        else if (frame.can_id & CAN_ERR_CNT)
            errorMsg += "TX or RX error counter class error.";
        errorlog(errorMsg, frame);
    }
    else
    {
        std::cout << "Standard format frame | ";
    }
}

bool SocketCAN::isCANUp()
{
    socket_ctrl = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ctrl < 0)
    {
        throw std::runtime_error("Error in opening control socket: " + std::string(strerror(errno)));
    }
    if (ioctl(socket_ctrl, SIOCGIFFLAGS, &ifr) < 0)
    {
        std::cerr << "Ioctl error: " << strerror(errno);
    }
    return (ifr.ifr_flags & IFF_UP) != 0;
}

int SocketCAN::initCAN(int bitrate)
{

    /* In case interface can0 is up */
    can_do_stop("can0");

    /* Set interface bitrate */
    if (can_set_bitrate("can0", bitrate) != 0)
    {
        std::cout << "Unable to set interface bitrate!" << std::endl;
        return -1;
    }
    /* Set interface up */
    if (can_do_start("can0") != 0)
    {
        std::cout << "Unable to start up the interface!" << std::endl;
        return -1;
    }
    else
        return 1;
}

std::string SocketCAN::checkState()
{
    int state, res;
    std::string bus_state = "";
    /* Get the state of the CAN interface */
    res = can_get_state(ifname, &state);

    if (res != 0)
        throw std::runtime_error("Error getting CAN state:" + std::string(strerror(errno)));

    /* Output the state of the CAN interface */
    switch (state)
    {
    case CAN_STATE_ERROR_ACTIVE:
        std::cout << "CAN state: ERROR_ACTIVE" << std::endl;
        bus_state += "ERROR ACTIVE STATE";
        break;
    case CAN_STATE_ERROR_WARNING:
        std::cout << "CAN state: ERROR_WARNING" << std::endl;
        bus_state += "ERROR WARNING STATE";
        break;
    case CAN_STATE_ERROR_PASSIVE:
        std::cout << "CAN state: ERROR_PASSIVE" << std::endl;
        bus_state += "ERROR PASSIVE STATE";
        break;
    case CAN_STATE_BUS_OFF:
        std::cout << "CAN state: BUS_OFF" << std::endl;
        bus_state += "BUS OFF STATE";
        break;
    case CAN_STATE_STOPPED:
        std::cout << "CAN state: STOPPED" << std::endl;
        bus_state += "STOPPED STATE";
        break;
    case CAN_STATE_SLEEPING:
        std::cout << "CAN state: SLEEPING" << std::endl;
        bus_state += "SLEEPING STATE";
        break;
    default:
        std::cout << "CAN state: UNKNOWN" << std::endl;
        bus_state += "UNKNOWN STATE";
        break;
    }
    return bus_state;
}
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

json Serial::serialreceive()
{
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
                    return json::parse(jsonString);
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

// TODO: Add header aka description of log file content 
// TIMESTAMP : ERROR DESCRIPTION
// ERROR FRAME CAN_ID PAYLOAD
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
            << std::setw(15) << std::hex << std::showbase << frame.can_id
            << std::setw(20) << "dlc:"
            << std::setw(5) << std::dec << std::showbase << (int)frame.can_dlc << "data: ";

    
    for (int i = 0; i < frame.can_dlc; ++i)
        {
        std::cout << std::hex << std::setw(2) << (int)frame.data[i] << " ";
        }
        std::cout << std::endl;
}
