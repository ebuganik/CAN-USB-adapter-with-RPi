#include "socketcan.h"
#include "serial.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>
#include <stdexcept>
#include <libsocketcan.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>

SocketCAN::SocketCAN(int bitrate)
{

    if (initCAN(bitrate) != 0)
    {
        std::cout << std::endl;
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
    // TODO: Check state once more even though it 'has been sent' (actually buffered)
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

struct can_frame SocketCAN::jsonunpack(const json &j)
{
    struct can_frame frame = {0};

    if (j["method"] == "write")
    {
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
        frame.can_id = val;
        frame.can_dlc = dlc;
        for (int i = 0; i < frame.can_dlc; i++)
        {

            frame.data[i] = data[i];
        }
        // TODO: Make a printf function for struct can_frame instead of this
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

    else /* condition for read method, in case it's necessary to extract filters */
    {

        std::vector<canid_t> canids;
        std::vector<canid_t> canmasks;

        std::string can_id = j["can_id"];
        std::string can_mask = j["can_mask"];
        std::string cln1 = can_id.substr(1, can_id.length() - 2);
        std::string cln2 = can_mask.substr(1, can_mask.length() - 2);
        std::stringstream ss1(cln1);
        std::stringstream ss2(cln2);
        std::string item1, item2;
        int conv_val1, conv_val2;
        while (std::getline(ss1, item1, ',') && std::getline(ss2, item2, ','))
        {
            std::stringstream(item1) >> std::hex >> conv_val1;
            std::stringstream(item2) >> std::hex >> conv_val2;
            canids.push_back(conv_val1);
            canmasks.push_back(conv_val2);
        }

        // Just to display, not neccessary 
        // std::cout << "CAN IDs: ";
        // for (const auto &val : canids)
        // {
        //     std::cout << std::hex << val << " ";
        // }
        // std::cout << "CAN masks: ";
        // for (const auto &val : canmasks)
        // {
        //     std::cout << std::hex << val << " ";
        // }

        std::vector<std::pair<canid_t, canid_t>> filters;
        for (size_t i = 0; i < canids.size(); ++i)
        {
            filters.emplace_back(canids[i], canmasks[i]);
        }
        canfilterEnable(filters);

        return frame;
    }
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

                if (checkState() == "ERROR ACTIVE STATE")
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
                else
                    frameAnalyzer(frame);
            }
        }
        return 1;
    }
}

void SocketCAN::canfilterEnable(std::vector<std::pair<canid_t, canid_t>> &filter)
{
    struct can_filter rfilter[filter.size()];
    for (size_t i = 0; i < filter.size(); ++i)
    {
        rfilter[i].can_id = filter[i].first;
        rfilter[i].can_mask = filter[i].second;
    }

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