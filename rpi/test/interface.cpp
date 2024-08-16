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

#define START_MARKER '{'
#define END_MARKER '}'
#define BUFFER_SIZE 200
using namespace std;


SocketCAN::SocketCAN(const std::string &interface_name, int bitrate)
{
    /*Neccessary structs
    - CAN interface info struct
    - CAN address info struct
    - CAN frame struct
    */

    /* Open a raw socket here */
    setCANUp(bitrate);
    socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (socket_fd < 0)
        throw std::runtime_error("Error in creating socket: " + std::string(strerror(errno)));

    /* Convert string interface_name to index */
    strcpy(ifr.ifr_name, interface_name.c_str());
    ioctl(socket_fd, SIOCGIFINDEX, &ifr);

    /* Setup address for binding */
    addr.can_ifindex = ifr.ifr_ifindex;
    addr.can_family = AF_CAN;

    /* Socket binding */
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

// Check if ok and send message
void SocketCAN::cansend(const struct can_frame &frame)
{
    if (write(socket_fd, &frame, sizeof(frame)) != sizeof(struct can_frame))
        throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
}

struct can_frame SocketCAN::jsonunpack(json j)
{
    // std::cout << "U funkciji jsonunpack ..." << std::endl;
    std::string method = j["method"];
    int bitrate = j["bitrate"];
    std::string id = j["can_id"];
    int val = stoi(id, 0, 16);
    int dlc = j["dlc"];
    std::cout << dlc << std::endl;
    std::string dataString = j["payload"];
    std::cout << "Data string: "<< dataString <<std::endl;
    std::vector<int> data;
    std::string cleanData = dataString.substr(1, dataString.length()-2);
    std::stringstream ss(cleanData);
    std::string item;
    int conv_val;

    while(std::getline(ss, item, ',')) {
        std::stringstream(item) >> std::hex >> conv_val;
        data.push_back(conv_val);
    }

    // Appply to can_frame struct
    struct can_frame frame;
    frame.can_id = val;
    frame.can_dlc = dlc; 
    std::cout << std::dec << std::showbase <<  (int)frame.can_dlc << std::endl;
    for (int i = 0; i < frame.can_dlc; i++)
        {

           frame.data[i] = data[i];
        }
    std::cout << "UNPACKED DATA INFO ..." << std::endl;
       std::cout << std::endl;
		
    std::cout << std::left << std::setw(15) << "interface:"
    << std::setw(10) << "can0" << std::setw(15) << "CAN ID:"
    << std::setw(10) << std::hex << std::showbase << frame.can_id
    << std::setw(20) << "data length:"
    << std::setw(5) <<std::dec<< std::showbase << (int)frame.can_dlc<< "data: ";

   for (int i = 0; i < frame.can_dlc; ++i)
     {
        std::cout << std::hex << std::showbase<< std::setw(2) << (int)frame.data[i] << " ";
     }
    std::cout << std::endl;
    return frame;
}
struct can_frame SocketCAN::canread()
{
    struct can_frame frame;
    int nbytes = read(socket_fd, &frame, sizeof(struct can_frame));
    if (nbytes < 0)
    {
        throw std::runtime_error("Receiving CAN frame failed: " + std::string(strerror(errno)));
    }

    if (nbytes < sizeof(struct can_frame))
    {
        throw std::runtime_error("Insufficient number of bytes received!");
    }
    return frame;
    
}

/* TODO: change this in case user wants to read frames with given CAN ID */
void SocketCAN::canfilterEnable()
{
    /* CAN reception filter */
    struct can_filter rfilter[2];

    /* Set up reception filter on this socket */
    rfilter[0].can_id = 0x123;
    rfilter[0].can_mask = CAN_SFF_MASK;
    rfilter[1].can_id = 0x200;
    rfilter[1].can_mask = 0x700;

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
    // can_err_mask_t err_mask = CAN_ERR_BUSOFF;
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
    /* Subclasses of control error frames
    If necessary, inform serial about CAN bus state */
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
        inform.serialsend("STATE: PASSIVE");
        break;
    case CAN_ERR_CRTL_TX_PASSIVE:
        errorDesc += "reached error passive status TX";
        inform.serialsend("STATE: PASSIVE");
        break;
    case CAN_ERR_CRTL_ACTIVE:
        errorDesc += "recovered to error active state";
        inform.serialsend("STATE: ACTIVE");

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
// From Frame Analyzer send bus state to serial, and error description in .log file
void SocketCAN::frameAnalyzer(const struct can_frame &frame)
{
    // Check if it's a remote transfer request frame, error frame or regular frame
    Serial inform;
    if (frame.can_id & CAN_RTR_FLAG)
    {
        std::cout << "RTR frame" << std::endl;
    }
    else if (frame.can_id & CAN_ERR_FLAG)
    {
        /* Check error class mask in can_id */
        // Message being sent to serial
        std::string errorMsg = " | CAN frame error | ";

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
            // Handling bus-off state
            std::cout << "RTR frame" << std::endl;
            inform.serialsend("STATE: BUS OFF");
            // setCANUp(250000);
            // sleep_for(10ns);
            system("sudo reboot");
        }
        else if (frame.can_id & CAN_ERR_BUSERROR)
            errorMsg += "Bus error (may flood!)";
        else if (frame.can_id & CAN_ERR_RESTARTED)
            errorMsg += "Controller restarted";
        else if (frame.can_id & CAN_ERR_CNT)
            // Add counter check
            errorMsg += "TX or RX error counter class error.";
        std::cout << errorMsg << std::endl;
    }
    else
    {
        std::cout << "Standard format frame" << std::endl;
    }
}

bool SocketCAN::isCANUp(const std::string &interface_name)
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

int SocketCAN::setCANUp(int bitrate)
{
    std::string set_down = "sudo ip link set can0 down";
    if (system(set_down.c_str()) < 0)
    {
        std::cout << "Unable to set interface down!" << std::endl;
        return -1;
    }
    std::string set_up = "sudo ip link set can0 up type can bitrate " + std::to_string(bitrate) +
                         " restart-ms 100";
    if (system(set_up.c_str()) < 0)
    {
        {
            std::cout << "Unable to set interface up!" << std::endl;
            return -1;
        }
    }
    return 0;
}

Serial::Serial()
{
    serial_fd = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_SYNC);
    if (serial_fd < 0)
        throw std::runtime_error("Failed to open serial port. Check if it is used by another device. " + std::string(strerror(errno)));
    
    struct termios config;
    
    // Get the current options for the port and set the baudrates to 115200   
    tcgetattr(serial_fd, &config);	
    cfsetispeed(&config, B115200);
    cfsetospeed(&config, B115200);

    // No parity (8N1)
    config.c_cflag &= ~PARENB;   
	config.c_cflag &= ~CSTOPB;   
	config.c_cflag &= ~CSIZE;	 
	config.c_cflag |=  CS8;

    // Hardware flow control disabled
    // Receiver enabled and local mode set
    config.c_cflag &= ~CRTSCTS;       
	config.c_cflag |= CREAD | CLOCAL; 
    // Software control disabled
    // Raw input (non-canonical), with no output processing
	config.c_iflag &= ~(IXON | IXOFF | IXANY);          
	config.c_iflag &=~(ECHO | ECHONL | ICANON | IEXTEN | ISIG);                          
    config.c_oflag &= ~OPOST;

    config.c_cc[VMIN]= 10;
    config.c_cc[VTIME]= 0;

    if (tcsetattr(serial_fd, TCSANOW, &config) < 0)
    {
        throw std::runtime_error("Unable to set serial configuration.\n");
    }
    tcflush(serial_fd, TCIFLUSH);   /* Discards old data in the rx buf  */
}

int Serial::getSerial()
{
    return serial_fd;
}

Serial::~Serial()
{
    close(serial_fd);
}

void Serial::sendJSON(const struct can_frame received)
{
   std::cout << "JSON to be sent ..."<<std::endl;
   json j;
   j["method"] = "canread";
   std::stringstream cc;
   cc << std::hex << std::setw(3) << std::showbase << received.can_id;
   j["can_id"] = cc.str();
   j["dlc"] = received.can_dlc;
   std::vector<std::string> payload_hex;
   for (int i = 0; i < received.can_dlc; ++i) {
        std::stringstream byte_ss;
	// std::cout << frame.data[i]<< std::endl; convert!
        byte_ss << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(received.data[i]);
        payload_hex.push_back(byte_ss.str());
    }

    // Joining data 
    std::stringstream payload_ss;
    payload_ss << "[";
    for (size_t i = 0; i < payload_hex.size(); ++i) {
        if (i != 0) {
            payload_ss << ",";
        }
	// std::cout << payload_hex[i];
        payload_ss << payload_hex[i];
    }
    payload_ss << "]";
    j["payload"] = payload_ss.str();
    std::string send_string = j.dump();
    // Call proper function to send string over serial
    std::cout << "String being sent to serial ...  "<< send_string;
    std::cout << send_string << std::endl;
    serialsend(send_string);
}

void Serial::serialsend(const std::string message)
{
    std::cout << "U funkciji serialsend sam ..." <<std::endl;
    const char *ch_message = message.c_str();
    std::cout << ch_message << std::endl;
    std::cout << "Slanje preko serijskog ..." << std::endl;
    int nbytes = write(serial_fd, ch_message, strlen(ch_message));
    if (nbytes < 0)
    {
        throw std::runtime_error("No bytes written to file. " + std::string(strerror(errno)));
    }
}

// Function needs to check whether JSON string is being received or not, with specified beginning and end
json Serial::serialreceive()
{
   char buf[BUFFER_SIZE];
   int buf_pos = 0;
   int bytes_read = 0;
   int json_started = 0;

   while(1) {
     bytes_read = read(serial_fd, &buf[buf_pos], 1);
        if (bytes_read > 0) {
            if (buf[buf_pos] == START_MARKER) {
                json_started = 1;
                buf_pos = 0;
            }

            if (json_started) {
                buf_pos += bytes_read;
                // std::cout << buf << std::endl;
            }

            if (buf[buf_pos - 1] == END_MARKER) {
                buf[buf_pos] = '\0'; 
                try {
                    std::cout << buf << std::endl;
                    std::string jsonString(buf);
                    return json::parse(jsonString);
                } catch (json::parse_error& e) {
                    std::cerr << "Parse error: " << e.what() << std::endl;
                }

                json_started = 0;
                buf_pos = 0;
                // Initialize integer array with zeros
                memset((void *)buf,'0',sizeof(buf));
            }
        }
        else
        {
            break;
        }
    }
   }
