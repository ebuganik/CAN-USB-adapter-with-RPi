#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>  // Needs to be included
#include <chrono>  

// Atomic flag to track if CTRL+C occured
std::atomic<bool> keepRunning(true);

// Thread functio simulator (like mine canThread)
void backgroundTask() {
    while (keepRunning) {
        std::cout << "Background task is running..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1)); // Simulate work
    }
    std::cout << "Background task is shutting down..." << std::endl;
}

// Signal handler for SIGINT (Ctrl+C)
void sigintHandler(int signal) {
    std::cout << "\nSIGINT received. Initiating shutdown..." << std::endl;
    keepRunning = false;  
}

int main() {
    // Set up the signal handler for SIGINT
    std::signal(SIGINT, sigintHandler);

    // Start the background task in a separate thread
    std::thread worker(backgroundTask);

    // Main loop that checks for Ctrl+C and runs the main program
    while (keepRunning) {
        std::cout << "Main thread is running..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2)); 
    }

    // Wait for the background thread to finish before exiting
    if (worker.joinable()) {
        worker.join();
    }

    std::cout << "Program safely terminated!" << std::endl;
    return 0;
}
