#pragma once

#include <chrono>
#include <cstdlib>

class timing {
    public:
        static void init(double factor=0);
        static std::chrono::milliseconds defaultWait;
        static std::chrono::milliseconds milliseconds(int value);
        static void sleep_for(std::chrono::milliseconds value);
        static double getFactor() { return sFactor; }
        static std::chrono::seconds maxTestTime;
    private:
        static double sFactor;
};
