#include "defaults.hpp"
#include <cstdlib>

std::chrono::milliseconds
defaultWaitTime(std::chrono::steady_clock::duration toAdd) {
    int waitMS = 100;
    if(const char* env_p = std::getenv("MQM_TEST_DEFAULT_WAIT_MS"))
        waitMS = std::atoi(env_p);

    std::chrono::milliseconds waitTime = std::chrono::milliseconds(waitMS);
    waitTime += std::chrono::duration_cast<std::chrono::milliseconds>(toAdd);

    return waitTime;
}
