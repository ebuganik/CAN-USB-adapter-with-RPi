#include "canhandler.h"
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

std::atomic<bool> cycleTimeRec(false);
CANHandler::CANHandler()
{

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

CANHandler::CANHandler(CANHandler &&other) : m_socketfd(other.m_socketfd)
{
    other.m_socketfd = -1;
}
CANHandler &CANHandler::operator=(CANHandler &&other) noexcept
{
    if (this != &other)
    {
        close(m_socketfd);
        m_socketfd = other.m_socketfd;
        other.m_socketfd = -1;
    }
    return *this;
}
CANHandler::~CANHandler()
{
    std::cout << "Calling destructor of CANHandler file desc" << std::endl;
    if (m_socketfd)
    {
        close(m_socketfd);
    }
}
void CANHandler::canSendPeriod(const struct can_frame &frame, int *cycle)
{
    Serial inform;
    int res = can_get_state(m_ifname, &m_state);
    if (m_state == CAN_STATE_BUS_OFF || m_state == CAN_STATE_ERROR_WARNING)
    {
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "Unable to send data, bus off! Restarting interface...");
        can_do_restart(m_ifname);
        return;
    }
    while (isRunning)
    {
        bool firstSend = true;
        {
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, []
                    { return cycleTimeRec.load(); });
        }
        if (!isRunning)
            break;

        while (isRunning)
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
                inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Sending CAN frame unsuccessful!");
                throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
                return;
            }
            else
            {
                if (firstSend)
                {
                    inform.sendStatusMessage(StatusCode::OPERATION_SUCCESS, "Sending CAN frame periodically.");
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
    int res = can_get_state(m_ifname, &m_state);
    if (m_state == CAN_STATE_BUS_OFF || m_state == CAN_STATE_ERROR_WARNING)
    {
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "Unable to send data, bus off! Restarting interface...");
        can_do_restart(m_ifname);
        return;
    }

    if (write(m_socketfd, &frame, sizeof(frame)) != sizeof(struct can_frame))
    {
        inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Sending CAN frame unsuccessful!");
        throw std::runtime_error("Sending CAN frame failed: " + std::string(strerror(errno)));
        return;
    }
    else
    {
        inform.sendStatusMessage(StatusCode::OPERATION_SUCCESS, "Sending CAN frame successful!");
        return;
    }
}
std::vector<int> CANHandler::parsePayload(const std::string &payload)
{
    std::vector<int> parsedData;
    std::istringstream stream(payload.substr(1, payload.length() - 2));
    std::string dataByte;

    while (std::getline(stream, dataByte, ','))
    {
        parsedData.push_back(std::stoi(dataByte, nullptr, 16));
    }

    return parsedData;
}
struct can_frame CANHandler::unpackWriteReq(const json &request)
{
    can_frame packedFrame;
    packedFrame.can_id = std::stoi(request["can_id"].get<std::string>(), nullptr, 16);
    packedFrame.can_dlc = request["dlc"];
    auto payload = request["payload"].get<std::string>();
    auto parsedPayload = parsePayload(payload);
    std::copy(parsedPayload.begin(), parsedPayload.begin() + packedFrame.can_dlc, packedFrame.data);
    displayFrame(packedFrame);
    return packedFrame;
}

void CANHandler::unpackFilterReq(const json &request)
{
    auto idString = request["can_id"].get<std::string>(), maskString = request["can_mask"].get<std::string>();
    auto parsedId = parsePayload(idString), parsedMask = parsePayload(maskString);
    std::vector<std::pair<int, int>> filterPair;
    for (size_t i = 0; i < parsedId.size(); i++)
    {
        filterPair.emplace_back(parsedId[i], parsedMask[i]);
    }

    canFilterEnable(filterPair);
}

int CANHandler::canRead()
{
    Serial inform;
    int res;
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
        inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Unable to set timeout, no bytes read from CAN bus!");
        return -1;
    }
    else if (retVal == 0)
    {
        std::cout << "No data within 10 seconds." << std::endl;
        inform.sendStatusMessage(StatusCode::TIMEOUT, "CONNECTION TIMEOUT. No activity on the socket.");
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
                res = can_get_state(m_ifname, &m_state);

                if (m_state == CAN_STATE_ERROR_ACTIVE)
                {
                    sleep(1);
                    /* In this case, it's probably SFF, RTR or EFF */
                    frameAnalyzer(readFrame);
                    displayFrame(readFrame);
                    inform.sendReadFrame(readFrame);
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

void CANHandler::canFilterEnable(std::vector<std::pair<int, int>> &filterPair)
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

int initCAN(int bitrate)
{

    /* In case interface can0 is up */
    can_do_stop("can0");

    /* Set interface bitrate */
    if (can_set_bitrate("can0", bitrate) != 0)
    {
        std::cout << "Unable to set interface bitrate!" << std::endl;
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

void CANHandler::processRequest(json &serialRequest, CANHandler &socket, struct can_frame &sendFrame, int *cycle)
{
    if (serialRequest["method"] == "write")
    {
        std::cout << "----------Write function detected----------" << std::endl;
        if (serialRequest.contains("cycle_ms"))
        {
            cycleTimeRec.store(true);
            *cycle = serialRequest["cycle_ms"];
        }
        else
        {
            cycleTimeRec.store(false);
        }

        sendFrame = socket.unpackWriteReq(serialRequest);

        if (cycleTimeRec)
        {
            std::cout << "Cycle time in ms: " << std::dec << cycle << std::endl;
            std::unique_lock<std::mutex> lock(m);
            initCAN(serialRequest["bitrate"]);
            CANHandler s;
            socket = move(s);
            cv.notify_one();
        }
        else
        {
            std::cout << "No cycle time added." << std::endl;
            initCAN(serialRequest["bitrate"]);
            CANHandler writeOp;
            writeOp.canSend(sendFrame);
        }
    }
    else if (serialRequest["method"] == "read")
    {
        {
            cycleTimeRec.store(false);
        }
        std::cout << "----------Read function detected----------" << std::endl;
        initCAN(serialRequest["bitrate"]);
        CANHandler readOp;
        if (serialRequest.contains("can_id") && serialRequest.contains("can_mask"))
        {
            readOp.unpackFilterReq(serialRequest);
        }
        readOp.errorFilter();
        readOp.canRead();
    }
    else
    {
        std::cout << "----------Invalid request-----------" << std::endl;
    }
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
