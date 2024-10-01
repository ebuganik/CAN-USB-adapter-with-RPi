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
    struct can_frame to_send = {0};
    int cycle = 0;
    json j;
    std::thread can_thread(&SocketCAN::cansendperiod, &socket, std::ref(to_send), &cycle);
    while (1)
    {
        std::thread serial_thread(&Serial::serialreceive, &serial, std::ref(j));
        serial_thread.join();

        if (j["method"] == "write")
        {
            std::cout << "----------Write function detected----------" << std::endl;
            if (j.contains("cycle_ms"))
            {
                cycle_ms_rec = true;
                cycle = j["cycle_ms"];
            }
            else
            {
                cycle_ms_rec = false;
            }

            to_send = socket.jsonunpack(j);

            if (cycle_ms_rec)
            {
                std::cout << "Cycle time in ms: " << std::dec<< cycle << std::endl;
                std::unique_lock<std::mutex> lock(m);
                close(socket.get_fd());
                SocketCAN socket(j["bitrate"]);
                socket.set_fd(-1);
                cv.notify_one();
            }
            else
            {
                std::cout << "No cycle time added." << std::endl;
                close(socket.get_fd());
                SocketCAN s(j["bitrate"]);
                socket.set_fd(s.get_fd());
                s.set_fd(-1);
                socket.cansend(to_send);
            }
        }
        else if (j["method"] == "read")
        {
            {
                cycle_ms_rec = false;
            }
            std::cout << "----------Read function detected----------" << std::endl;
            close(socket.get_fd());
            SocketCAN s(j["bitrate"]);
            socket.set_fd(s.get_fd());
            s.set_fd(-1);
            if (j.contains("can_id") && j.contains("can_mask"))
            {
                socket.jsonunpack(j);
            }
            socket.canread();
        }
        else
        {
            std::cout << "----------Invalid request-----------" << std::endl;
        }
    }
    return 0;
}