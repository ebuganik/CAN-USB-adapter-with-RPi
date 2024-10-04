#include "socketcan.h"
#include "serial.h"
#include <iostream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/can/raw.h>
#include <stdexcept>
#include <libsocketcan.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <chrono>
#include <thread>
#include <atomic>


using namespace std;
using namespace std::chrono;

std::mutex m;
std::condition_variable cv;
std::atomic<bool> dataReady;

CANHandler::CANHandler(int bitrate)
{

    // TODO: Change constructor. Exception to error code
    
    if (initCAN(bitrate) != 0)
    {
        std::cout << "can0 interface set to bitrate " << std::dec<< bitrate << std::endl;
    }
    else
        throw std::runtime_error("Error in initializing CAN interface!");

    m_socketfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketfd < 0)
        throw std::runtime_error("Error in creating socket: " + std::string(strerror(errno)));

    strcpy(m_ifr.ifr_name, m_ifname);
    ioctl(m_socketfd, SIOCGIFINDEX, &m_ifr);

    m_addr.can_ifindex = m_ifr.ifr_ifindex;
    m_addr.can_family = AF_CAN;

    if (bind(m_socketfd, (struct sockaddr *)&m_addr, sizeof(m_addr)) < 0)
    {
        throw std::runtime_error("Error in socket binding: " + std::string(strerror(errno)));
    }
}

CANHandler::~CANHandler()
{
    if (m_socketfd != -1)
    {
        close(m_socketfd);
    }
}
void CANHandler::canSendPeriod(const struct can_frame &frame, int *cycle)
{
    Serial inform;
    if (checkState() == "BUS OFF STATE" || checkState() == "BUS WARNING STATE")
    {
        inform.serialSend("Unable to send data, bus off! Restarting interface...\n");
        can_do_restart(m_ifname);
        return;
    }
    while (1)
    {
        bool firstSend = true;

        {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, []
                    { return cycleTimeRec; });
        }

        while (1)
        {
            {
                std::unique_lock<std::mutex> lock(m);
                if (dataReady)
                {
                    dataReady = false;
                    break;
                }
            }

            if (write(m_socketfd, &frame, sizeof(frame)) != sizeof(struct can_frame))
            {
                inform.serialSend("NACK: Sending CAN frame unsuccessful!\n");
                throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
                return;
            }
            else
            {
                if (firstSend)
                {
                    inform.serialSend("ACK: Sending CAN frame periodically.\n");
                    firstSend = false;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(*cycle));
        }
    }
}
void CANHandler::canSend(const struct can_frame &frame)
{
    Serial inform;
    if (checkState() == "BUS OFF STATE" || checkState() == "BUS WARNING STATE")
    {
        inform.serialSend("Unable to send data, bus off! Restarting interface...\n");
        can_do_restart(m_ifname);
        return;
    }

    if (write(m_socketfd, &frame, sizeof(frame)) != sizeof(struct can_frame))
    {
        inform.serialSend("NACK: Sending CAN frame unsuccessful!\n");
        throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
        return;
    }
    else
    {
        inform.serialSend("ACK: Sending CAN frame successful!\n");
        return;
    }
}

// TODO: Minimize function
struct can_frame CANHandler::jsonUnpack(const json &request)
{
    struct can_frame frame = {0};

    if (request["method"] == "write")
    {
        std::string id = request["can_id"];
        int val = stoi(id, 0, 16);
        int dlc = request["dlc"];
        std::string dataString = request["payload"];
        std::vector<int> data;
        std::string cleanData = dataString.substr(1, dataString.length() - 2);
        std::stringstream stringStream(cleanData);
        std::string item;
        int convValue;

        while (std::getline(stringStream, item, ','))
        {
            std::stringstream(item) >> std::hex >> convValue;
            data.push_back(convValue);
        }

        /* Appply json parameters to can_frame struct */
        frame.can_id = val;
        frame.can_dlc = dlc;
        for (int i = 0; i < frame.can_dlc; i++)
        {

            frame.data[i] = data[i];
        }
        displayFrame(frame);
        return frame;
    }

    else /* condition for read method, in case it's necessary to extract filterPair */
    {
        // TODO: this part of code can be used for another function e.g. extractFilter
        std::vector<canid_t> canids, canmasks;
        std::string can_id = request["can_id"], can_mask = request["can_mask"];
        std::string cleanData1 = can_id.substr(1, can_id.length() - 2);
        std::string cleanData2 = can_mask.substr(1, can_mask.length() - 2);
        std::stringstream stringStream1(cleanData1), stringStream2(cleanData2);
        std::string item1, item2;
        int convVal1, convVal2;
        while (std::getline(stringStream1, item1, ',') && std::getline(stringStream2, item2, ','))
        {
            std::stringstream(item1) >> std::hex >> convVal1;
            std::stringstream(item2) >> std::hex >> convVal2;
            canids.push_back(convVal1);
            canmasks.push_back(convVal2);
        }
        std::vector<std::pair<canid_t, canid_t>> filterPair;
        for (size_t i = 0; i < canids.size(); ++i)
        {
            filterPair.emplace_back(canids[i], canmasks[i]);
        }
        canFilterEnable(filterPair);

        return frame;
    }
}

int CANHandler::canRead()
{
    Serial inform;
    struct can_frame readFrame;
    /* To set timeout in read function */
    struct timeval timeout;
    fd_set set;
    int retVal;

    FD_ZERO(&set);
    FD_SET(m_socketfd, &set);

    /* 10 seconds timeout */
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    retVal = select(m_socketfd + 1, &set, NULL, NULL, &timeout);

    if (retVal == -1)
    {
        std::cout << "An error ocurred in timeout set up!" << std::endl;
        inform.serialSend("Unable to set timeout, no bytes read from CAN bus!");
        return -1;
    }
    else if (retVal == 0)
    {
        std::cout << "No data within 10 seconds." << std::endl;
        inform.serialSend("CONNECTION TIMEOUT. No activity on the socket.");
        return -1;
    }
    else
    {
        auto startTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedTime;
        while (1)
        {

            /* Reads one frame at the time*/
            if (read(m_socketfd, &readFrame, sizeof(struct can_frame)))
            {

                if (checkState() == "ERROR ACTIVE STATE")
                {
                    sleep(1);
                    /* In this case, it's probably SFF, RTR or EFF */
                    frameAnalyzer(readFrame);
                    displayFrame(readFrame);
                    inform.sendJson(readFrame);
                    break;
                }
                /* This part checks error frames */
                else
                {
                    sleep(1);
                    frameAnalyzer(readFrame);
                    displayFrame(readFrame);
                    auto currTime = std::chrono::steady_clock::now();
                    elapsedTime = currTime - startTime;
                }
            }
            if (elapsedTime.count() >= 5.00)
            {
                std::cout << "Reading time of error frames elapsed!" << std::endl;
                break;
            }
        }

        return 1;
    }
}

void CANHandler::canFilterEnable(std::vector<std::pair<canid_t, canid_t>> &filterPair)
{
    struct can_filter recFilter[filterPair.size()];
    for (size_t i = 0; i < filterPair.size(); ++i)
    {
        recFilter[i].can_id = filterPair[i].first;
        recFilter[i].can_mask = filterPair[i].second;
    }
    // TODO: exception to error code
    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_FILTER, &recFilter, sizeof(recFilter)) < 0)
        throw std::runtime_error("Unable to set reception filter: " + std::string(strerror(errno)));
}

void CANHandler::canFilterDisable()
{
    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0) < 0)
        throw std::runtime_error("Unable to reset reception filter: " + std::string(strerror(errno)));
}

void CANHandler::errorFilter()
{

    can_err_mask_t errMask = CAN_ERR_MASK;
    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &errMask, sizeof(errMask)) < 0)
        throw std::runtime_error("Unable to set error filter: " + std::string(strerror(errno)));
}

void CANHandler::loopBack(int mode)
{
    /* Local loopBack is enabled by default and can be separately disabled for every socket
    When enabled, it is possible to receive your own messages */
    /* 0 - disabled, 1 - enabled */
    if (mode != 1)
    {
        setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &mode, sizeof(mode));
    }
    else
    {
        int recv_own_msgs = 1; /* 0 = disabled (default), 1 = enabled */
        setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_LOOPBACK, &mode, sizeof(mode));
        setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_RECV_OWN_MSGS, &recv_own_msgs, sizeof(recv_own_msgs));
    }
}

std::string CANHandler::getCtrlErrorDesc(__u8 ctrlError)
{
    /* Subclasses of control error frames */

    std::string errorDesc = "";
    Serial inform;
    switch (ctrlError)
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
        inform.serialSend("ERROR PASSIVE RX STATE\n");
        break;
    case CAN_ERR_CRTL_TX_PASSIVE:
        errorDesc += "reached error passive status TX";
        inform.serialSend("ERROR PASSIVE TX STATE\n");
        break;
    case CAN_ERR_CRTL_ACTIVE:
        errorDesc += "recovered to error active state";
        inform.serialSend("ERROR ACTIVE STATE\n");

        break;
    default:
        errorDesc += "unknown error state";
    }
    return errorDesc;
}

std::string CANHandler::getProtErrorTypeDesc(__u8 protError)
{
    std::string errorDesc = "";
    switch (protError)
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

std::string CANHandler::getProtErrorLocDesc(__u8 protError)
{
    std::string errorDesc = "";
    switch (protError)
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

std::string CANHandler::getTransceiverStatus(__u8 statusError)
{
    std::string errorDesc = "";
    switch (statusError)
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

void CANHandler::frameAnalyzer(const struct can_frame &frame)
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
            /* bus off check */
            if (frame.can_id & CAN_ERR_BUSOFF)
            {
                errorMsg += "- BUS OFF";
                inform.serialSend("BUS OFF STATE");
                can_do_restart(m_ifname);
            }
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
            inform.serialSend("BUS OFF STATE");
            can_do_restart(m_ifname);
        }
        else if (frame.can_id & CAN_ERR_BUSERROR)
            errorMsg += "Bus error (may flood!)";
        else if (frame.can_id & CAN_ERR_RESTARTED)
            errorMsg += "Controller restarted";
        else if (frame.can_id & CAN_ERR_CNT)
            errorMsg += "TX or RX error counter class error.";
    }
    else
    {
        std::cout << "Standard format frame | ";
    }
}

int CANHandler::initCAN(int bitrate)
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
// TODO: Don't return std::string from function. Exception to error code
std::string CANHandler::checkState()
{
    int state, res;
    std::string bus_state = "";
    /* Get the state of the CAN interface */
    res = can_get_state(m_ifname, &state);

    if (res != 0)
        throw std::runtime_error("Error getting CAN state:" + std::string(strerror(errno)));

    /* Output the state of the CAN interface */
    switch (state)
    {
    case CAN_STATE_ERROR_ACTIVE:
        bus_state += "ERROR ACTIVE STATE";
        break;
    case CAN_STATE_ERROR_WARNING:
        bus_state += "ERROR WARNING STATE";
        break;
    case CAN_STATE_ERROR_PASSIVE:
        bus_state += "ERROR PASSIVE STATE";
        break;
    case CAN_STATE_BUS_OFF:
        // TODO: restart
        bus_state += "BUS OFF STATE";
        break;
    case CAN_STATE_STOPPED:
        bus_state += "STOPPED STATE";
        break;
    case CAN_STATE_SLEEPING:
        bus_state += "SLEEPING STATE";
        break;
    default:
        bus_state += "UNKNOWN STATE";
        break;
    }
    return bus_state;
}

void CANHandler::displayFrame(const struct can_frame &frame)
{
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
}

int CANHandler::getFd() const
{
    return m_socketfd;
}

void CANHandler::setFd(int t_socketfd)
{
    m_socketfd = t_socketfd;
}
