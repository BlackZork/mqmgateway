#include "timing.hpp"

#include <thread>

#include "libmodmqttsrv/logging.hpp"

double timing::sFactor = 1;

void 
timing::init() {
    // slow tests in release build by default
    // to support slow ARM machines
    #ifdef NDEBUG
        sFactor = 10;
    #endif

    if(const char* env_p = std::getenv("MQM_TEST_TIMING_FACTOR")) {
        sFactor = std::stod(env_p);
    }

    if (sFactor != 1)
        spdlog::info("Time-dependent tests slowed down x{}", sFactor);

    if(const char* env_p = std::getenv("MQM_TEST_DEBUG_WAIT_SEC")) {
        defaultWait = std::chrono::milliseconds(std::atoi(env_p));   
        spdlog::info("server.waitFor... set to {}", defaultWait);          
    } 
}

std::chrono::milliseconds 
timing::milliseconds(int value) {
    std::chrono::milliseconds ret = std::chrono::milliseconds(value);
    ret *= sFactor;
    return ret;
}

void
timing::sleep(std::chrono::milliseconds value) {
    std::this_thread::sleep_for(value * sFactor);
}

std::chrono::milliseconds timing::defaultWait = std::chrono::seconds(5);
