#include "serial.h"
#include "socketcan.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <time.h>
#include <cstdlib>
#include <thread>
#include <atomic>

using namespace std;
bool cycle_ms_rec;

int main()
{
    Serial serial;
    SocketCAN socket(200000);
    int cycle = 0;
    json j;
    std::thread can_thread(&SocketCAN::cansendperiod, &socket, &cycle);
    while (1)
    {
        std::thread serial_thread(&Serial::serialreceive, &serial, std::ref(j));
        serial_thread.join();

        if (j["method"] == "write")
        {
            std::cout << "---Write function detected---" << std::endl;
            if (j.contains("cycle_ms"))
            {
                cycle_ms_rec = true;
                cycle = j["cycle_ms"];
            }

            if (cycle_ms_rec)
            {
                std::unique_lock<std::mutex> lock(m);
                close(socket.get_fd());
                SocketCAN socket(j["bitrate"]);
                socket.set_fd(-1);
                cv.notify_one();
            }
            else
            {
                {
                    cycle_ms_rec = false;
                    cycle = 0;
                }
                std::cout << "---Write once---" << std::endl;
                close(socket.get_fd());
                SocketCAN socket(j["bitrate"]);
                socket.set_fd(-1);
                socket.cansend(&cycle);
            }
        }
        else
        {
            {
                cycle_ms_rec = false;
            }
            std::cout << "---Read function detected---" << std::endl;
            close(socket.get_fd());
            SocketCAN s(j["bitrate"]);
            socket.set_fd(s.get_fd());
            s.set_fd(-1);
            socket.canread();
        }
    }
    return 0;
}