#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

int main()
{
	struct ifreq ifr;				/* CAN interface info struct */
	struct sockaddr_can addr;		/* CAN adddress info struct */
	struct can_frame frame;			/* CAN frame struct */
	struct can_filter rfilter[2];	/* CAN reception filter */
	int s;							/* SocketCAN handle */

	memset(&ifr, 0, sizeof(ifr));
	memset(&addr, 0, sizeof(addr));
	memset(&frame, 0, sizeof(frame));
	
	// TODO: Open a socket here
	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);
	
	/* Convert interface string "can0" to index */
	strcpy(ifr.ifr_name, "can0");
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	/* Setup address for binding */
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_family = AF_CAN;

	can_err_mask_t err_mask = CAN_ERR_MASK;
    // can_err_mask_t err_mask = CAN_ERR_BUSOFF;
    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0)
       {
        printf("Unable to set error filter!");
       }
	
	// TODO: Bind socket to can0 interface
	bind(s, (struct sockaddr *)&addr, sizeof(addr));
	
	while(1)
	{
		// TODO: Read received frame and print on console
		read(s, &frame, sizeof(frame));
		printf("can0\t\t\t%x\t[%d]\t%x %x %x %x %x %x %x %x\n",frame.can_id, frame.can_dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3], frame.data[4],frame.data[5], frame.data[6], frame.data[7] );  
		
	}
	
	close(s);
	
	return 0;
}
