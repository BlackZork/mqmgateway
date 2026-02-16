#pragma once

#include <chrono>
#include <cstdlib>

class timing {
    public:
        static void init();
        static std::chrono::milliseconds defaultWait;
        static std::chrono::milliseconds milliseconds(int value);
        static void sleep(std::chrono::milliseconds value);
    private:
        static double sFactor;
};