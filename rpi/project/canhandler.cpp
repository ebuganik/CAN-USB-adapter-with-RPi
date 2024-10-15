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

/* Constructor of class CANHandler used to initialize serial communication */

CANHandler::CANHandler(const char *ifname)
{

    syslog(LOG_DEBUG, "Initializing socket communication.");
    m_ifname = ifname;
    m_socketfd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketfd < 0)
    {
        syslog(LOG_ERR, "Error in creating socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    strcpy(m_ifr.ifr_name, m_ifname);
    ioctl(m_socketfd, SIOCGIFINDEX, &m_ifr);

    m_addr.can_ifindex = m_ifr.ifr_ifindex;
    m_addr.can_family = AF_CAN;

    if (bind(m_socketfd, (struct sockaddr *)&m_addr, sizeof(m_addr)) < 0)
    {
        syslog(LOG_ERR, "Error in socket binding: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/* CANHandler move constructor */

CANHandler::CANHandler(CANHandler &&other) : m_socketfd(other.m_socketfd)
{
    other.m_socketfd = -1;
}

/* CANHandler move assignment constructor */

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
        syslog(LOG_DEBUG, "Calling CANHandler class destructor.");
        close(m_socketfd);
    }
}

/* This function sends CAN frame periodically, for given cycle parameter */

void CANHandler::canSendPeriod(const struct can_frame &frame, int *cycle)
{
    syslog(LOG_DEBUG, "Started canSendPeriod function call.");
    Serial inform;
    int res = can_get_state(m_ifname, &m_state);
    if (m_state == CAN_STATE_BUS_OFF || m_state == CAN_STATE_ERROR_WARNING)
    {
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "Unable to send data, BUS OFF! Restarting interface...");
        syslog(LOG_ALERT, "Node in BUS OFF or ERROR WARNING state. Restart required.");
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
        {
            syslog(LOG_DEBUG, "Exiting canSendPeriod function call. SIGINT received.");
            break;
        }

        while (isRunning)
        {
            {
                std::unique_lock<std::mutex> lock(m);
                if (dataReady)
                {
                    syslog(LOG_DEBUG, "Exiting canSendPeriod function call. dataReady flag set to TRUE.");
                    dataReady = false;
                    break;
                }
            }

            if (write(m_socketfd, &frame, sizeof(frame)) != sizeof(struct can_frame))
            {
                inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Sending CAN frame unsuccessful!");
                syslog(LOG_ERR, "Sending CAN frame failed: %s", strerror(errno));
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
            {
                syslog(LOG_DEBUG, "Exiting canSendPeriod function call. SIGINT received.");
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(*cycle));
        }
    }
}

/* This function does sending CAN frame once */

void CANHandler::canSend(const struct can_frame &frame)
{
    syslog(LOG_DEBUG, "Started canSend function call.");
    Serial inform;
    int res = can_get_state(m_ifname, &m_state);
    if (m_state == CAN_STATE_BUS_OFF || m_state == CAN_STATE_ERROR_WARNING)
    {
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "Unable to send data, bus off! Restarting interface...");
        syslog(LOG_ALERT, "Node in BUS OFF or ERROR WARNING state. Restart required.");
        can_do_restart(m_ifname);
        return;
    }

    if (write(m_socketfd, &frame, sizeof(frame)) != sizeof(struct can_frame))
    {
        inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Sending CAN frame unsuccessful!");
        syslog(LOG_ERR, "Sending CAN frame failed: %s", strerror(errno));
        return;
    }
    else
    {
        blinkLed(17, 1000);
        inform.sendStatusMessage(StatusCode::OPERATION_SUCCESS, "Sending CAN frame successful!");
        syslog(LOG_DEBUG, "Exiting canSend function call after successful write.");
        return;
    }
}
/* This function does string payload parsing into std::vector array of ints */

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
/* This function unpacks write serial request parameters to set up can_frame for sending onto CAN BUS */

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

/* This function unpacks pairs of CAN IDs and CAN masks from received serial request and enables filtering read */

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

/* This function does reading from CAN BUS */
int CANHandler::canRead()
{
    syslog(LOG_DEBUG, "Started canRead function call.");
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
        inform.sendStatusMessage(StatusCode::OPERATION_ERROR, "Unable to set timeout, no bytes read from CAN bus!");
        syslog(LOG_WARNING, "Timeout not set, no bytes read from CAN bus.");
        return -1;
    }
    else if (retVal == 0)
    {
        inform.sendStatusMessage(StatusCode::TIMEOUT, "CONNECTION TIMEOUT. No activity on the socket.");
        syslog(LOG_NOTICE, "No data within 10 s. Timeout expired.");
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
                    syslog(LOG_INFO, "Node in ERROR active state and able to read from CAN BUS");
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
                    syslog(LOG_WARNING, "Node in state of reading ERROR frames.");
                    sleep(1);
                    frameAnalyzer(readFrame);
                    displayFrame(readFrame);
                    auto currTime = std::chrono::steady_clock::now();
                    elapsedTime = currTime - startTime;
                }
            }
            if (elapsedTime.count() >= 5.00)
            {
                syslog(LOG_INFO, "Reading time of ERROR frames elapsed.");
                break;
            }
        }
        syslog(LOG_DEBUG, "Exiting canRead function after successful read.");
        return 1;
    }
}

void CANHandler::canFilterEnable(std::vector<std::pair<int, int>> &filterPair)
{
    syslog(LOG_DEBUG, "Entering canFilterEnable function.");
    struct can_filter recFilter[filterPair.size()];
    for (size_t i = 0; i < filterPair.size(); ++i)
    {
        recFilter[i].can_id = filterPair[i].first;
        recFilter[i].can_mask = filterPair[i].second;
    }

    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_FILTER, &recFilter, sizeof(recFilter)) < 0)
    {
        syslog(LOG_ERR, "Unable to set reception filter: %s", strerror(errno));
    }

    else
    {
        syslog(LOG_INFO, "Reception filter set successfully.");
    }
    syslog(LOG_DEBUG, "Exiting canFilterEnable function.");
}

void CANHandler::canFilterDisable()
{
    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0) < 0)
        syslog(LOG_ERR, "Unable to reset reception filter: %s", strerror(errno));
}

void CANHandler::errorFilter()
{
    syslog(LOG_DEBUG, "Entering errorFilter function.");
    can_err_mask_t errMask = CAN_ERR_MASK;
    if (setsockopt(m_socketfd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &errMask, sizeof(errMask)) < 0)
    {
        syslog(LOG_ERR, "Unable to set error filter: %s", strerror(errno));
    }

    else
    {
        syslog(LOG_INFO, "Error filter set successfully.");
    }
    syslog(LOG_DEBUG, "Exiting errorFilter function.");
}
// TODO: finish loopBack call and logging messages

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
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_CRTL_RX_OVERFLOW:
        errorMessage += "RX buf overflow";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_CRTL_TX_OVERFLOW:
        errorMessage += "TX buf overflow";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_CRTL_RX_WARNING:
        errorMessage += "reached warning level for RX errors";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_CRTL_TX_WARNING:
        errorMessage += "reached warning level for TX errors";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_CRTL_RX_PASSIVE:
        errorMessage += "reached error passive status RX";
        syslog(LOG_ERR, errorMessage.c_str());
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "ERROR PASSIVE RX STATE\n");
        break;
    case CAN_ERR_CRTL_TX_PASSIVE:
        errorMessage += "reached error passive status TX";
        syslog(LOG_ERR, errorMessage.c_str());
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "ERROR PASSIVE TX STATE\n");
        break;
    case CAN_ERR_CRTL_ACTIVE:
        errorMessage += "recovered to error active state";
        syslog(LOG_INFO, errorMessage.c_str());
        inform.sendStatusMessage(StatusCode::NODE_STATUS, "ERROR ACTIVE STATE\n");

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
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_BIT:
        errorMessage += "single bit error";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_FORM:
        errorMessage += "frame format error";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_STUFF:
        errorMessage += "bit stuffing error";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_BIT0:
        errorMessage += "unable to send dominant bit";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_BIT1:
        errorMessage += "unable to send recessive bit";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_OVERLOAD:
        errorMessage += "bus overload";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_ACTIVE:
        errorMessage += "active error announcement";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_TX:
        errorMessage += "error occured on transmission";
        syslog(LOG_ERR, errorMessage.c_str());
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
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_SOF:
        errorMessage += "start of frame";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ID28_21:
        errorMessage += "ID bits 28-21 (SFF 10-3)";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ID20_18:
        errorMessage += "ID bits 20 - 18 (SFF: 2 - 0 )";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_SRTR:
        errorMessage += "substitute RTR (SFF: RTR)";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_IDE:
        errorMessage += "identifier extension";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ID17_13:
        errorMessage += "ID bits 17-13";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ID12_05:
        errorMessage += "ID bits 12-5";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ID04_00:
        errorMessage += "ID bits 4-0";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_RTR:
        errorMessage += "RTR";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_RES1:
        errorMessage += "reserved bit 1";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_RES0:
        errorMessage += "reserved bit 0";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_DLC:
        errorMessage += "data length code";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_DATA:
        errorMessage += "data section";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_CRC_SEQ:
        errorMessage += "CRC sequence";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_CRC_DEL:
        errorMessage += "CRC delimiter";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ACK:
        errorMessage += "ACK slot";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_ACK_DEL:
        errorMessage += "ACK delimiter";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_EOF:
        errorMessage += "end of frame";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_PROT_LOC_INTERM:
        errorMessage += "intermission";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    default:
        errorMessage += "unknown error state";
        syslog(LOG_ERR, errorMessage.c_str());
    }
    return;
}

void CANHandler::getTransceiverStatus(unsigned char statusError, std::string &errorMessage)
{
    switch (statusError)
    {
    case CAN_ERR_TRX_UNSPEC:
        errorMessage += "unspecified";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANH_NO_WIRE:
        errorMessage += "CANH no wire";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_BAT:
        errorMessage += "CANH short to BAT";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_VCC:
        errorMessage += "CANH short to VCC";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANH_SHORT_TO_GND:
        errorMessage += "CANH short to GND";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANL_NO_WIRE:
        errorMessage += "CANL no wire";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_BAT:
        errorMessage += "CANL short to BAT";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_VCC:
        errorMessage += "CANL short to VCC";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_GND:
        errorMessage += "CANL short to GND";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    case CAN_ERR_TRX_CANL_SHORT_TO_CANH:
        errorMessage += "CANL short to CANH";
        syslog(LOG_ERR, errorMessage.c_str());
        break;
    default:
        errorMessage += "unknown error state";
        syslog(LOG_ERR, errorMessage.c_str());
    }
    return;
}

/* Analyze type of frame received */

void CANHandler::frameAnalyzer(const struct can_frame &frame)
{
    Serial inform;
    if (frame.can_id & CAN_RTR_FLAG)
    {
        syslog(LOG_INFO, "Remote Transmission Request frame received.");
    }
    else if (frame.can_id & CAN_EFF_FLAG)
    {
        syslog(LOG_INFO, "Extended Format Frame received.");
    }
    else if (frame.can_id & CAN_ERR_FLAG)
    {
        std::string errorMsg = "";

        if (frame.can_id & CAN_ERR_TX_TIMEOUT)
        {
            errorMsg += "TX Timeout";
            syslog(LOG_ERR, errorMsg.c_str());
        }
        else if (frame.can_id & CAN_ERR_LOSTARB)
        {
            errorMsg += "Lost arbitration";
            if (frame.data[0] == CAN_ERR_LOSTARB_UNSPEC)
            {
                errorMsg += "... bit unspecified";
                syslog(LOG_ERR, errorMsg.c_str());
            }
        }
        else if (frame.can_id & CAN_ERR_CRTL)
        {
            errorMsg += "Controller problems - ";
            getCtrlErrorDesc(frame.data[1], errorMsg);
            /* Bus off check in case if BUS off happened also  */
            if (frame.can_id & CAN_ERR_BUSOFF)
            {
                errorMsg += "- BUS OFF";
                inform.sendStatusMessage(StatusCode::NODE_STATUS, "BUS OFF STATE");
                syslog(LOG_ALERT, "Node in BUS OFF state. Restart required.");
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
        {
            errorMsg += "Received no ACK on transmission";
            syslog(LOG_ERR, errorMsg.c_str());
        }
        else if (frame.can_id & CAN_ERR_BUSOFF)
        {
            inform.sendStatusMessage(StatusCode::NODE_STATUS, "BUS OFF STATE");
            syslog(LOG_ALERT, "Node in BUS OFF or ERROR WARNING state. Restart required.");
            can_do_restart(m_ifname);
        }
        else if (frame.can_id & CAN_ERR_BUSERROR)
        {
            errorMsg += "Bus error (may flood!)";
            syslog(LOG_ALERT, errorMsg.c_str());
            can_do_restart(m_ifname);
        }
        else if (frame.can_id & CAN_ERR_RESTARTED)
        {
            errorMsg += "Controller restarted";
            syslog(LOG_ERR, errorMsg.c_str());
        }
        else if (frame.can_id & CAN_ERR_CNT)
        {
            errorMsg += "TX or RX error counter class error.";
            syslog(LOG_ERR, errorMsg.c_str());
        }
    }
    else
    {
        syslog(LOG_INFO, "Standard Format Frame received.");
    }
}

int initCAN(int bitrate)
{
    syslog(LOG_DEBUG, "Shutting down CAN interface %s", interfaceName);
    can_do_stop(interfaceName);

    syslog(LOG_DEBUG, "Configuring bitrate for CAN interface %s to %d", interfaceName, bitrate);
    if (can_set_bitrate(interfaceName, bitrate) != 0)
    {
        syslog(LOG_ERR, "Unable to configure bitrate for CAN interface %s!", interfaceName);
    }

    syslog(LOG_DEBUG, "Setting up CAN interface %s.", interfaceName);
    if (can_do_start(interfaceName) != 0)
    {
        syslog(LOG_ERR, "Failed to set up CAN interface %s!", interfaceName);
        return -1;
    }
    else
        return 1;
}

void CANHandler::processRequest(json &serialRequest, CANHandler &socket, struct can_frame &sendFrame, int *cycle)
{
    syslog(LOG_DEBUG, "Started processRequest function call.");
    if (serialRequest["method"] == "write")
    {
        syslog(LOG_INFO, "Write function detected.");
        if (serialRequest.contains("cycle_ms"))
        {
            cycleTimeRec.store(true);
            *cycle = serialRequest["cycle_ms"];
            syslog(LOG_INFO, "Cycle time in ms added: %d", *cycle);
        }
        else
        {
            cycleTimeRec.store(false);
            syslog(LOG_INFO, "No cycle time added.");
        }

        sendFrame = socket.unpackWriteReq(serialRequest);

        if (cycleTimeRec)
        {
            std::unique_lock<std::mutex> lock(m);
            initCAN(serialRequest["bitrate"]);
            CANHandler s("can0");
            socket = move(s);
            cv.notify_one();
        }
        else
        {
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

        syslog(LOG_INFO, "Read function detected.");
        initCAN(serialRequest["bitrate"]);
        CANHandler readOp(interfaceName);
        if (serialRequest.contains("filter_enable") && serialRequest["filter_enable"] == "YES")
        {
            readOp.unpackFilterReq(serialRequest);
        }
        readOp.errorFilter();
        readOp.canRead();
    }
    syslog(LOG_DEBUG, "Exiting processRequest function call.");
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
