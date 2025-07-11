#pragma once

#include <chrono>

std::chrono::milliseconds
defaultWaitTime(std::chrono::steady_clock::duration toAdd = std::chrono::steady_clock::duration::zero());

class TestNumbers {
    public:
        static constexpr uint32_t ABCD = 0xA1B2C3D4;
        static constexpr uint32_t CDAB = 0xC3D4A1B2;
        static constexpr uint32_t DCBA = 0xD4C3B2A1;
        static constexpr uint32_t BADC = 0xB2A1D4C3;

        static constexpr uint16_t AB = 0xA1B2;
        static constexpr uint16_t BA = 0xB2A1;
        static constexpr uint16_t CD = 0xC3D4;
        static constexpr uint16_t DC = 0xD4C3;

        static constexpr int32_t ABCD_as_int32 = -1582119980;
        static constexpr int32_t CDAB_as_int32 = -1009475150;
        static constexpr int32_t DCBA_as_int32 = -725372255;
        static constexpr int32_t BADC_as_int32 = -1298017085;

        static constexpr float ABCD_as_float = (float)0xA1B2C3D4;
        static constexpr float CDAB_as_float = (float)0xC3D4A1B2;
        static constexpr float DCBA_as_float = (float)0xD4C3B2A1;
        static constexpr float BADC_as_float = (float)0xB2A1D4C3;
};