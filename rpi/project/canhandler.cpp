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
std::atomic<bool> dataReady(false);

std::atomic<bool> cycleTimeRec(false);
CANHandler::CANHandler(const char* ifname)
{

    m_ifname = ifname;
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
                    { return cycleTimeRec.load() || !isRunning.load(); });
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
                blinkLed(17, 1000);
                if (firstSend)
                {
                    inform.sendStatusMessage(StatusCode::OPERATION_SUCCESS, "Sending CAN frame periodically.");
                    firstSend = false;
                }
            }
            if (!isRunning)
                break;
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
        blinkLed(17, 1000);
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
                blinkLed(17, 1000);
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

void CANHandler::getCtrlErrorDesc(unsigned char ctrlError, std::string &errorMessage)
{
    /* Subclasses of control error frames */
    Serial inform;
    switch (ctrlError)
    {
    case CAN_ERR_CRTL_UNSPEC:
        errorMessage += "unspecified";
        break;
    case CAN_ERR_CRTL_RX_OVERFLOW:
        errorMessage += "RX buf overflow";
        break;
    case CAN_ERR_CRTL_TX_OVERFLOW:
        errorMessage += "TX buf overflow";
        break;
    case CAN_ERR_CRTL_RX_WARNING:
        errorMessage += "reached warning level for RX errors";
        break;
    case CAN_ERR_CRTL_TX_WARNING:
        errorMessage += "reached warning level for TX errors";
        break;
    case CAN_ERR_CRTL_RX_PASSIVE:
        errorMessage += "reached error passive status RX";
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "ERROR PASSIVE RX STATE\n");
        break;
    case CAN_ERR_CRTL_TX_PASSIVE:
        errorMessage += "reached error passive status TX";
        inform.sendStatusMessage(StatusCode::NODE_STATUS,"ERROR PASSIVE TX STATE\n");
        break;
    case CAN_ERR_CRTL_ACTIVE:
        errorMessage += "recovered to error active state";
        inform.sendStatusMessage(StatusCode::NODE_STATUS,"ERROR ACTIVE STATE\n");

        break;
    default:
        errorMessage += "unknown error state";
    }
    return;
}

void CANHandler::getProtErrorTypeDesc(unsigned char protError, std::string &errorMessage)
{
    switch (protError)
    {
    case CAN_ERR_PROT_UNSPEC:
        errorMessage += "unspecified";
        break;
    case CAN_ERR_PROT_BIT:
        errorMessage += "single bit error";
        break;
    case CAN_ERR_PROT_FORM:
        errorMessage += "frame format error";
        break;
    case CAN_ERR_PROT_STUFF:
        errorMessage += "bit stuffing error";
        break;
    case CAN_ERR_PROT_BIT0:
        errorMessage += "unable to send dominant bit";
        break;
    case CAN_ERR_PROT_BIT1:
        errorMessage += "unable to send recessive bit";
        break;
    case CAN_ERR_PROT_OVERLOAD:
        errorMessage += "bus overload";
        break;
    case CAN_ERR_PROT_ACTIVE:
        errorMessage += "active error announcement";
    case CAN_ERR_PROT_TX:
        errorMessage += "error occured on transmission";
        break;
    default:
        errorMessage += "unknown error state";
    }
    return;
}

void CANHandler::getProtErrorLocDesc(unsigned char protError, std::string &errorMessage)
{
    switch (protError)
    {
    case CAN_ERR_PROT_LOC_UNSPEC:
        errorMessage += "unspecified";
        break;
    case CAN_ERR_PROT_LOC_SOF:
        errorMessage += "start of frame";
        break;
    case CAN_ERR_PROT_LOC_ID28_21:
        errorMessage += "ID bits 28-21 (SFF 10-3)";
        break;
    case CAN_ERR_PROT_LOC_ID20_18:
        errorMessage += "ID bits 20 - 18 (SFF: 2 - 0 )";
        break;
    case CAN_ERR_PROT_LOC_SRTR:
        errorMessage += "substitute RTR (SFF: RTR)";
        break;
    case CAN_ERR_PROT_LOC_IDE:
        errorMessage += "identifier extension";
        break;
    case CAN_ERR_PROT_LOC_ID17_13:
        errorMessage += "ID bits 17-13";
        break;
    case CAN_ERR_PROT_LOC_ID12_05:
        errorMessage += "ID bits 12-5";
        break;
    case CAN_ERR_PROT_LOC_ID04_00:
        errorMessage += "ID bits 4-0";
        break;
    case CAN_ERR_PROT_LOC_RTR:
        errorMessage += "RTR";
        break;
    case CAN_ERR_PROT_LOC_RES1:
        errorMessage += "reserved bit 1";
        break;
    case CAN_ERR_PROT_LOC_RES0:
        errorMessage += "reserved bit 0";
        break;
    case CAN_ERR_PROT_LOC_DLC:
        errorMessage += "data length code";
        break;
    case CAN_ERR_PROT_LOC_DATA:
        errorMessage += "data section";
        break;
    case CAN_ERR_PROT_LOC_CRC_SEQ:
        errorMessage += "CRC sequence";
        break;
    case CAN_ERR_PROT_LOC_CRC_DEL:
        errorMessage += "CRC delimiter";
        break;
    case CAN_ERR_PROT_LOC_ACK:
        errorMessage += "ACK slot";
        break;
    case CAN_ERR_PROT_LOC_ACK_DEL:
        errorMessage += "ACK delimiter";
        break;
    case CAN_ERR_PROT_LOC_EOF:
        errorMessage += "end of frame";
        break;
    case CAN_ERR_PROT_LOC_INTERM:
        errorMessage += "intermission";
        break;
    default:
        errorMessage += "unknown error state";
    }
    return;
}

void CANHandler::getTransceiverStatus(unsigned char statusError, std::string &errorMessage)
{
    switch (statusError)
    {
    case CAN_ERR_TRX_UNSPEC:
        errorMessage += "unspecified";
        break;
    case CAN_ERR_TRX_CANH_NO_WIRE:
        errorMessage += "CANH no wire";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_BAT:
        errorMessage += "CANH short to BAT";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_VCC:
        errorMessage += "CANH short to VCC";
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_GND:
        errorMessage += "CANH short to GND";
        break;
    case CAN_ERR_TRX_CANL_NO_WIRE:
        errorMessage += "CANL no wire";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_BAT:
        errorMessage += "CANL short to BAT";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_VCC:
        errorMessage += "CANL short to VCC";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_GND:
        errorMessage += "CANL short to GND";
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_CANH:
        errorMessage += "CANL short to CANH";
        break;
    default:
        errorMessage += "unknown error state";
    }
    return;
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
            getCtrlErrorDesc(frame.data[1], errorMsg);
            /* Bus off check */
            if (frame.can_id & CAN_ERR_BUSOFF)
            {
                errorMsg += "- BUS OFF";
                inform.sendStatusMessage(StatusCode::NODE_STATUS, "BUS OFF STATE");
                can_do_restart(m_ifname);
            }
        }
        else if (frame.can_id & CAN_ERR_PROT)
        {
            errorMsg += "Protocol violations - ";
            getProtErrorTypeDesc(frame.data[2], errorMsg);
            getProtErrorLocDesc(frame.data[3], errorMsg);
        }

        else if (frame.can_id & CAN_ERR_TRX)
        {
            errorMsg += "Transciever status - ";
            getTransceiverStatus(frame.data[4], errorMsg);
        }
        else if (frame.can_id & CAN_ERR_ACK)
            errorMsg += "Received no ACK on transmission";
        else if (frame.can_id & CAN_ERR_BUSOFF)
        {
            std::cout << "BUS OFF ... Restarting can0 interface" << std::endl;
            inform.sendStatusMessage(StatusCode::NODE_STATUS, "BUS OFF STATE");
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
            CANHandler s("can0");
            socket = move(s);
            cv.notify_one();
        }
        else
        {
            std::cout << "No cycle time added." << std::endl;
            initCAN(serialRequest["bitrate"]);
            CANHandler writeOp(interfaceName);
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
        CANHandler readOp(interfaceName);
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
 void CANHandler::blinkLed(int led, int time)
{
    digitalWrite(led, HIGH);
    delay(time);
    digitalWrite(led, LOW);
    delay(time);
    
}
