#pragma once

#include <cstdint>

namespace TestNumbers {

class Int {
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

        static constexpr uint32_t ABCD_as_uint32 = 2712847316;
        static constexpr uint32_t CDAB_as_uint32 = 3285492146;
        static constexpr uint32_t DCBA_as_uint32 = 3569595041;
        static constexpr uint32_t BADC_as_uint32 = 2996950211;

};

class Float {
    public:
        //-1.234567
        static constexpr uint32_t ABCD = 0xBF9E064B;
        static constexpr uint32_t CDAB = 0x064BBF9E;
        static constexpr uint32_t DCBA = 0x4B069EBF;
        static constexpr uint32_t BADC = 0x9EBF4B06;

        static constexpr uint16_t AB = 0xBF9E;
        static constexpr uint16_t BA = 0x9EBF;
        static constexpr uint16_t CD = 0x064B;
        static constexpr uint16_t DC = 0x4B06;

        static const float ABCD_as_float;
        static const float CDAB_as_float;
        static const float DCBA_as_float;
        static const float BADC_as_float;
};

} //namespace