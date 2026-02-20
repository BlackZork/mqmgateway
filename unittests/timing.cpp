#include "timing.hpp"

#include <thread>
#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/logging.hpp"

double timing::sFactor = 1;
std::chrono::seconds timing::maxTestTime = std::chrono::seconds(10);

void
timing::init(double factor) {
    defaultWait = std::chrono::seconds(5);
    maxTestTime = std::chrono::seconds(10);

    // slow tests in release build by default
    // to support slow ARM machines
    if (factor == 0) {
        #ifdef NDEBUG
            sFactor = 10;
        else
            sFactor = 1;
        #endif

        if(const char* env_p = std::getenv("MQM_TEST_TIMING_FACTOR")) {
            sFactor = std::stod(env_p);
        }
    } else {
        sFactor = factor;
    }

    defaultWait *= sFactor;
    maxTestTime *= sFactor;

    if (sFactor != 1)
        spdlog::info("Time-dependent tests slowed down x{}", sFactor);

}

std::chrono::milliseconds
timing::milliseconds(int value) {
    std::chrono::milliseconds ret = std::chrono::milliseconds(value);
    ret *= sFactor;
    return ret;
}

void
timing::sleep_for(std::chrono::milliseconds value) {
    auto tgtval = value * sFactor;
    spdlog::trace("Sleeping for {}", tgtval);
    std::this_thread::sleep_for(tgtval);
}

std::chrono::milliseconds timing::defaultWait = std::chrono::seconds(5);
