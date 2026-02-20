#pragma once

#include <chrono>
#include <cstdlib>

class timing {
    public:
        class milliseconds {
            public:
                static std::chrono::milliseconds zero() { return std::chrono::milliseconds::zero(); }
        };

        static void init(double factor=0);
        static std::chrono::milliseconds defaultWait;
        static std::chrono::milliseconds milliseconds(int value);
        static std::chrono::seconds seconds(int value);
        static double getFactor() { return sFactor; }
        static std::chrono::seconds maxTestTime;
        static std::chrono::milliseconds substract(std::chrono::time_point<std::chrono::steady_clock> left, std::chrono::time_point<std::chrono::steady_clock> right) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(left - right);
        }
    private:
        static double sFactor;
};
