#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <iostream>
// Add to test vector of pairs
#include <vector>
#include <utility>

using namespace std;
int main()
{
	struct ifreq ifr;				
	struct sockaddr_can addr;		
	struct can_frame frame;			
	// struct can_filter rfilter[2];	
	int s, retval;						

	/* Add to use timeout */ 
	struct timeval timeout;
	fd_set set;
	
	/* Initialization */
	memset(&ifr, 0, sizeof(ifr));
	memset(&addr, 0, sizeof(addr));
	memset(&frame, 0, sizeof(frame));
	
	/* Opening socket */
	s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

	/* Convert interface string "can0" to index */
	strcpy(ifr.ifr_name, "can0");
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	/* Setup address for binding */
	addr.can_ifindex = ifr.ifr_ifindex;
	addr.can_family = AF_CAN;

	can_err_mask_t err_mask = CAN_ERR_MASK;
    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask, sizeof(err_mask)) < 0)
       {
        printf("Unable to set error filter!");
       }

	std::vector<std::pair<canid_t, canid_t>> masknfilter = {{0x123, CAN_SFF_MASK}, {0x200, 0x700}};
	struct can_filter rfilter[masknfilter.size()];
      for (size_t i = 0; i < masknfilter.size(); ++i)
      {
        rfilter[i].can_id = masknfilter[i].first;
        rfilter[i].can_mask = masknfilter[i].second;
      }
	// rfilter[0].can_id   = 0x123;
	// rfilter[0].can_mask = CAN_SFF_MASK;
	// rfilter[1].can_id   = 0x200;
	// rfilter[1].can_mask = 0x700;


    if (setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter, sizeof(rfilter)) < 0)
      { printf("Unable to set reception filter!");
	  }
	
	/* Bind socket to can0 interface */
	bind(s, (struct sockaddr *)&addr, sizeof(addr));

	FD_ZERO(&set);
	FD_SET(s, &set);

	timeout.tv_sec = 10;
	timeout.tv_usec = 0;

	retval = select(s+1, &set, NULL, NULL, &timeout);
	
	// while(1)
	// {
		
	// 	read(s, &frame, sizeof(frame));
	// 	printf("can0\t\t\t%x\t[%d]\t%x %x %x %x %x %x %x %x\n",frame.can_id, frame.can_dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3], frame.data[4],frame.data[5], frame.data[6], frame.data[7] );  
		
	// }

	 if (retval == -1) {
        printf("select error");
    } else if (retval) {
		printf("Ovdje sam...\n");
		while(1)
		{
         if (read(s, &frame, sizeof(struct can_frame))) {
			printf("can0\t\t\t%x\t[%d]\t%x %x %x %x %x %x %x %x\n",frame.can_id, frame.can_dlc, frame.data[0], frame.data[1], frame.data[2], frame.data[3], frame.data[4],frame.data[5], frame.data[6], frame.data[7] );
			break;
        }
		}
    } else printf("Isteklo 10 sekundi.\n");
	
	close(s);
	
	return 0;
}
