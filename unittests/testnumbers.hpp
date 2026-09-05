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

        static constexpr int16_t AB_as_int16 = -24142;
        static constexpr int16_t BA_as_int16 = -19807;

        static constexpr uint16_t AB_as_uint16 = 41394;
        static constexpr uint16_t BA_as_uint16 = 45729;

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

        // special values: two-register (high, low) pairs
        static constexpr uint16_t NAN_HIGH     = 0x7FC0;
        static constexpr uint16_t NAN_LOW      = 0x0000;
        static constexpr uint16_t POS_INF_HIGH = 0x7F80;
        static constexpr uint16_t POS_INF_LOW  = 0x0000;
        static constexpr uint16_t NEG_INF_HIGH = 0xFF80;
        static constexpr uint16_t NEG_INF_LOW  = 0x0000;
};

class Int64 {
    public:
        // the four register orders a converter can be asked for: as read, with the
        // whole word order reversed, with the bytes of every register swapped, and
        // with both.
        static constexpr uint64_t ABCDEFGH = 0xA1B2C3D4E5F61728;
        static constexpr uint64_t GHEFCDAB = 0x1728E5F6C3D4A1B2;
        static constexpr uint64_t BADCFEHG = 0xB2A1D4C3F6E52817;
        static constexpr uint64_t HGFEDCBA = 0x2817F6E5D4C3B2A1;

        static constexpr uint16_t AB = 0xA1B2;
        static constexpr uint16_t BA = 0xB2A1;
        static constexpr uint16_t CD = 0xC3D4;
        static constexpr uint16_t DC = 0xD4C3;
        static constexpr uint16_t EF = 0xE5F6;
        static constexpr uint16_t FE = 0xF6E5;
        static constexpr uint16_t GH = 0x1728;
        static constexpr uint16_t HG = 0x2817;

        static constexpr int64_t ABCDEFGH_as_int64 = -6795153568590063832;
        static constexpr int64_t GHEFCDAB_as_int64 = 1668836509950976434;
        static constexpr int64_t BADCFEHG_as_int64 = -5574940925582039017;
        static constexpr int64_t HGFEDCBA_as_int64 = 2889049152959001249;

        // ABCDEFGH is above INT64_MAX, so it only survives an unsigned holder
        static constexpr uint64_t ABCDEFGH_as_uint64 = 11651590505119487784u;
        static constexpr uint64_t GHEFCDAB_as_uint64 = 1668836509950976434u;
        static constexpr uint64_t BADCFEHG_as_uint64 = 12871803148127512599u;
        static constexpr uint64_t HGFEDCBA_as_uint64 = 2889049152959001249u;
};

class Double {
    public:
        //-1.2345678901234567
        static constexpr uint64_t ABCDEFGH = 0xBFF3C0CA428C59FB;
        static constexpr uint64_t GHEFCDAB = 0x59FB428CC0CABFF3;
        static constexpr uint64_t BADCFEHG = 0xF3BFCAC08C42FB59;
        static constexpr uint64_t HGFEDCBA = 0xFB598C42CAC0F3BF;

        static constexpr uint16_t AB = 0xBFF3;
        static constexpr uint16_t BA = 0xF3BF;
        static constexpr uint16_t CD = 0xC0CA;
        static constexpr uint16_t DC = 0xCAC0;
        static constexpr uint16_t EF = 0x428C;
        static constexpr uint16_t FE = 0x8C42;
        static constexpr uint16_t GH = 0x59FB;
        static constexpr uint16_t HG = 0xFB59;

        static const double ABCDEFGH_as_double;
        static const double GHEFCDAB_as_double;
        static const double BADCFEHG_as_double;
        static const double HGFEDCBA_as_double;

        // special values: four registers, most significant first
        static constexpr uint16_t NAN_REGISTERS[] = {0x7FF8, 0x0000, 0x0000, 0x0000};
        static constexpr uint16_t POS_INF_REGISTERS[] = {0x7FF0, 0x0000, 0x0000, 0x0000};
        static constexpr uint16_t NEG_INF_REGISTERS[] = {0xFFF0, 0x0000, 0x0000, 0x0000};
};

} //namespace
