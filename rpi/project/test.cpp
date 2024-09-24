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
    int cycle = 0, bitrate;
    json j;
    std::thread can_thread(&SocketCAN::cansend, &socket, &cycle);
    while (1)
    {
        std::thread serial_thread(&Serial::serialreceive, &serial, std::ref(j));
        serial_thread.join();

        if (j["method"] == "write")
        {
            std::cout << "Write function detected" << std::endl;
            if (j.contains("cycle_ms"))
            {
                cycle_ms_rec = true;
                // cv.notify_one();
                std::cout << "cycle_ms in main:" << std::boolalpha << cycle_ms_rec << std::endl;
                cycle = j["cycle_ms"];
            }
            bitrate = j["bitrate"];

            if (cycle_ms_rec)
            {
                std::unique_lock<std::mutex> lock(m);
                close(socket.get_fd());
                SocketCAN s(bitrate);
                socket = s;
                s.set_fd(-1);
                cv.notify_one();
            }
            else
            {
                // TODO: adjust cansend for sending only one
            }
        }
        else
        {
            {
                cycle_ms_rec = false;
                std::cout << "cycle_ms_rec in read request: " << std::boolalpha<< cycle_ms_rec << std::endl;
            }
            std::cout << "Read function detected" << std::endl;
            close(socket.get_fd());
            SocketCAN s(j["bitrate"]);
            socket = s;
            s.set_fd(-1);
            socket.canread();
        }
    }
    return 0;
}