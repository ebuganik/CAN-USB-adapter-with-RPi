#include <iostream>
#include <linux/can.h>
#include <libsocketcan.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    const char *ifname = "can0";
    int bitrate = 250000;  
    int state;
    int ret;

    // Set interface up and it's bitrate, but first set interface down if it is not

    if (can_do_stop(ifname)!= 0) {
        std::cout << "Unable to set interface down!"<< std::endl;
        return 1;
    }
    sleep(1);
    // After setting bitrate!!!
   /* if(can_do_start(ifname)!= 0){
        std::cout << "Unable to set interface up!"<< std::endl;
        return 1;
    }*/
     if(can_set_bitrate(ifname, bitrate)!= 0){
        std::cout << "Unable to set bitrate!"<< std::endl;
        return 1;
    }
     if(can_do_start(ifname)!= 0){
        std::cout << "Unable to set interface up!"<< std::endl;
        return 1;
    }
    if (can_get_state(ifname, &state)!= 0) {
        std::cout << "Error getting CAN state: "  << std::endl;
        return 1;
    }

    switch (state) {
        case CAN_STATE_ERROR_ACTIVE:
            std::cout << "CAN state: ERROR_ACTIVE" << std::endl;
            break;
        case CAN_STATE_ERROR_WARNING:
            std::cout << "CAN state: ERROR_WARNING" << std::endl;
            break;
        case CAN_STATE_ERROR_PASSIVE:
            std::cout << "CAN state: ERROR_PASSIVE" << std::endl;
            break;
        case CAN_STATE_BUS_OFF:
            std::cout << "CAN state: BUS_OFF" << std::endl;
            break;
        case CAN_STATE_STOPPED:
            std::cout << "CAN state: STOPPED" << std::endl;
            break;
        case CAN_STATE_SLEEPING:
            std::cout << "CAN state: SLEEPING" << std::endl;
            break;
        default:
            std::cout << "CAN state: UNKNOWN" << std::endl;
            break;
    }

    return 0;
}
