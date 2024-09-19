#include <iostream>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <condition_variable>

std::mutex m;
std::condition_variable cv;

bool data_ready = false;
bool cycle_ms_received = false;
int thread_index_value = 0;

void serial_function() {
	printf("Serial Thread: Start\n");
	
	int i = 5;
	while(i--) {
		std::cout << "Serial Thread: " << i << std::endl;
		
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	}

	{ std::unique_lock<std::mutex> lock(m);
		data_ready = true;
		++thread_index_value;
	}
}

void can_function() {
	printf("CAN Thread: Start\n");
	while(1) {
		printf("CAN Thread: Next Iteration\n");
	    { std::unique_lock<std::mutex> lock(m);
			cv.wait(lock, [] { return cycle_ms_received; });
		}

		while(1) {
			{ std::unique_lock<std::mutex> lock(m);
				if(data_ready) {
					data_ready = false;
					break;
				}
			}
			
			std::cout << "CAN Thread: " << thread_index_value << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}
}

int main() {
	bool valid = true;

	std::thread can_thread(can_function);

	while(valid) {
		std::thread serial_thread(serial_function);
		serial_thread.join();

		{ // Check if cycle_ms received.
			cycle_ms_received = true;
		}

		if(cycle_ms_received) {
			{ std::unique_lock<std::mutex> lock(m);
				cv.notify_one();
			}
		}
	}
	
	return 0;
}
