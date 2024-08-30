#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <iostream>
#include <string.h>
#include "../ext_lib/json.hpp"

#define START_MARKER '{'
#define END_MARKER '}'
#define BUFFER_SIZE 200


using namespace std;
using json = nlohmann::json;
using namespace nlohmann::literals;

int configure_fd(const char *portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        perror("Error opening serial port");
        return -1;
    }

    struct termios SerialPortSettings;	

	tcgetattr(fd, &SerialPortSettings);	

	cfsetispeed(&SerialPortSettings,B115200);
	cfsetospeed(&SerialPortSettings,B115200); 

	SerialPortSettings.c_cflag &= ~PARENB;   /* Disables the Parity Enable bit(PARENB),So No Parity   */
	SerialPortSettings.c_cflag &= ~CSTOPB;   /* CSTOPB = 2 Stop bits cleared, so 1 Stop bit */
	SerialPortSettings.c_cflag &= ~CSIZE;	 /* Clears the mask for setting the data size             */
	SerialPortSettings.c_cflag |=  CS8;      /* Set the data bits = 8                                 */
	SerialPortSettings.c_cflag &= ~CRTSCTS;       /* No Hardware flow Control                         */
	SerialPortSettings.c_cflag |= CREAD | CLOCAL; /* Enable receiver,ignore Modem Control lines       */
	SerialPortSettings.c_iflag &= ~(IXON | IXOFF | IXANY);          /* Disable XON/XOFF flow control both i/p and o/p */
	SerialPortSettings.c_lflag &= ~(ECHO | ECHONL | ICANON|  IEXTEN | ISIG);  /* Non Cannonical mode                            */

	SerialPortSettings.c_oflag &= ~OPOST; /*No Output Processing*/
    
    SerialPortSettings.c_cc[VMIN]  = 10;
    SerialPortSettings.c_cc[VTIME] = 0;

	if((tcsetattr(fd,TCSANOW,&SerialPortSettings)) != 0){ /* Set the attributes to the termios structure*/
		printf("\n  Error in setting attributes");
		return -1;
	}else{
		printf("\n  BaudRate = 115200 \n  StopBits = 1 \n  Parity   = none\n");
	}
    return fd;
}

void read_json_from_serial(int fd) {
    char buffer[BUFFER_SIZE];
    int buffer_pos = 0;
    int bytes_read;
    int json_started = 0;

    while (1) {
        bytes_read = read(fd, &buffer[buffer_pos], 1);
        if (bytes_read > 0) {
            if (buffer[buffer_pos] == START_MARKER) {
                json_started = 1;
                buffer_pos = 0;
            }

            if (json_started) {
                buffer_pos += bytes_read;
                // std::cout << buffer << std::endl;
            }

            if (buffer[buffer_pos - 1] == END_MARKER) {
                buffer[buffer_pos] = '\0'; 
                try {
                    std::cout << buffer << std::endl;
                    std::string jsonString(buffer);
                    json jsonParsed = json::parse(jsonString);
                    std::cout << "Received JSON: " << jsonParsed.dump() << std::endl; 
                    std::cout << jsonParsed["method"] << std::endl;
                } catch (json::parse_error& e) {
                    std::cerr << "Parse error: " << e.what() << std::endl;
                }

                json_started = 0;
                buffer_pos = 0;
                // Initialize integer array with zeros
                memset((void *)buffer,'0',sizeof(buffer));
            }
        }
    }
}

int main() {
    const char *portname = "/dev/ttyS0"; 
    int fd = configure_fd(portname);

    if (fd < 0) {
        return 1;
    }

    read_json_from_serial(fd);

    close(fd);
    return 0;
}
