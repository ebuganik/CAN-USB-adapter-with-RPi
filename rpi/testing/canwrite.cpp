#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <libsocketcan.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <iostream>
#include <linux/can.h>
#include <linux/can/raw.h>

using namespace std;

void checkState(int state)
{
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
			can_do_restart("can0");
			std::cout << "Restarting interface ..." << std::endl;
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
}


int main()
{
	int state;
	struct ifreq ifr;			/* CAN interface info struct */
	struct sockaddr_can addr;	/* CAN adddress info struct */
	struct can_frame frame;		/* CAN frame struct */
	int s;						/* SocketCAN handle */

	memset(&ifr, 0, sizeof(ifr));
	memset(&addr, 0, sizeof(addr));
	memset(&frame, 0, sizeof(frame));
	
	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	
	/* Convert interface string "can0" to index */
	strcpy(ifr.ifr_name, "can0");
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	/* Setup address for binding */
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_family = AF_CAN;
	
	/* Disable reception filter on this RAW socket */
	setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);
	
	bind(s, (struct sockaddr *)&addr, sizeof(addr));
	
	while(1)
	{
		if (can_get_state("can0", &state)!= 0) {
        std::cout << "Error getting CAN state: "  << std::endl;
        return 1;
    }
		checkState(state);
		frame.can_id = 0x123;
		frame.can_dlc = 8;
		frame.data[0] = 0x01;
		frame.data[1] = 0x04;
		frame.data[2] = 0x22;
		frame.data[3] = 0x34;
		frame.data[4] = 0x6;

		write(s, &frame, sizeof(frame));
	}

	close(s);
	
	return 0;
}
