#include <catch2/catch_all.hpp>

#include <cmath>
#include <cstring>

#include "libmodmqttconv/convtools.hpp"

#include "testnumbers.hpp"

TEST_CASE("ConverterTools::registersToNumber should") {

    SECTION("combine four registers into an int64") {
        const std::vector<uint16_t> input({TestNumbers::Int64::AB, TestNumbers::Int64::CD,
                                           TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(ConverterTools::registersToNumber<int64_t>(input, false, false) == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("reverse the whole word order when low_first is set") {
        const std::vector<uint16_t> input({TestNumbers::Int64::AB, TestNumbers::Int64::CD,
                                           TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(ConverterTools::registersToNumber<int64_t>(input, true, false) == TestNumbers::Int64::GHEFCDAB_as_int64);
    }

    SECTION("swap the bytes of every register before combining them") {
        const std::vector<uint16_t> input({TestNumbers::Int64::BA, TestNumbers::Int64::DC,
                                           TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        REQUIRE(ConverterTools::registersToNumber<int64_t>(input, false, true) == TestNumbers::Int64::ABCDEFGH_as_int64);
    }

    SECTION("reverse words and swap bytes together") {
        const std::vector<uint16_t> input({TestNumbers::Int64::BA, TestNumbers::Int64::DC,
                                           TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        REQUIRE(ConverterTools::registersToNumber<int64_t>(input, true, true) == TestNumbers::Int64::GHEFCDAB_as_int64);
    }

    SECTION("keep a value above INT64_MAX when read as unsigned") {
        const std::vector<uint16_t> input({TestNumbers::Int64::AB, TestNumbers::Int64::CD,
                                           TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(ConverterTools::registersToNumber<uint64_t>(input, false, false) == TestNumbers::Int64::ABCDEFGH_as_uint64);
        REQUIRE(TestNumbers::Int64::ABCDEFGH_as_uint64 > uint64_t(INT64_MAX));
    }

    SECTION("combine two registers into an int32") {
        const std::vector<uint16_t> input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        REQUIRE(ConverterTools::registersToNumber<int32_t>(input, false, false) == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(ConverterTools::registersToNumber<int32_t>(input, true, false) == TestNumbers::Int::CDAB_as_int32);
    }

    SECTION("read a single register") {
        const std::vector<uint16_t> input({TestNumbers::Int::AB});

        REQUIRE(ConverterTools::registersToNumber<uint16_t>(input, false, false) == TestNumbers::Int::AB_as_uint16);
        REQUIRE(ConverterTools::registersToNumber<int16_t>(input, false, false) == TestNumbers::Int::AB_as_int16);
        REQUIRE(ConverterTools::registersToNumber<int16_t>(input, false, true) == TestNumbers::Int::BA_as_int16);
    }

    SECTION("zero extend when fewer registers than the width are given") {
        const std::vector<uint16_t> input({TestNumbers::Int::AB});

        // never sign extended, whatever the top bit of the register says
        REQUIRE(ConverterTools::registersToNumber<int32_t>(input, false, false) == int32_t(TestNumbers::Int::AB));
        REQUIRE(ConverterTools::registersToNumber<int64_t>(input, false, false) == int64_t(TestNumbers::Int::AB));
    }

    SECTION("ignore registers above the width of the value") {
        const std::vector<uint16_t> input({TestNumbers::Int::AB, TestNumbers::Int::CD, TestNumbers::Int::AB});

        REQUIRE(ConverterTools::registersToNumber<int32_t>(input, false, false) == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(ConverterTools::registersToNumber<int32_t>(input, true, false) == TestNumbers::Int::CDAB_as_int32);
    }

    SECTION("return zero for no registers at all") {
        REQUIRE(ConverterTools::registersToNumber<int32_t>(std::vector<uint16_t>(), false, false) == 0);
    }
}

TEST_CASE("ConverterTools::numberToRegisters should") {

    SECTION("split an int64 into four registers") {
        const std::vector<uint16_t> expected({TestNumbers::Int64::AB, TestNumbers::Int64::CD,
                                              TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, false, false, 4) == expected);
    }

    SECTION("reverse the whole word order when low_first is set") {
        const std::vector<uint16_t> expected({TestNumbers::Int64::GH, TestNumbers::Int64::EF,
                                              TestNumbers::Int64::CD, TestNumbers::Int64::AB});

        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, true, false, 4) == expected);
    }

    SECTION("swap the bytes of every register") {
        const std::vector<uint16_t> expected({TestNumbers::Int64::BA, TestNumbers::Int64::DC,
                                              TestNumbers::Int64::FE, TestNumbers::Int64::HG});

        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, false, true, 4) == expected);
    }

    SECTION("reverse words and swap bytes together") {
        const std::vector<uint16_t> expected({TestNumbers::Int64::HG, TestNumbers::Int64::FE,
                                              TestNumbers::Int64::DC, TestNumbers::Int64::BA});

        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, true, true, 4) == expected);
    }

    SECTION("write a value above INT64_MAX unchanged") {
        const std::vector<uint16_t> expected({TestNumbers::Int64::AB, TestNumbers::Int64::CD,
                                              TestNumbers::Int64::EF, TestNumbers::Int64::GH});

        REQUIRE(ConverterTools::numberToRegisters<uint64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_uint64, false, false, 4) == expected);
    }

    SECTION("keep the least significant words when asked for fewer registers") {
        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, false, false, 2) == std::vector<uint16_t>({TestNumbers::Int64::EF, TestNumbers::Int64::GH}));
        REQUIRE(ConverterTools::numberToRegisters<int64_t>(
                    TestNumbers::Int64::ABCDEFGH_as_int64, false, false, 1) == std::vector<uint16_t>({TestNumbers::Int64::GH}));
    }

    SECTION("sign extend a negative value into the padding registers") {
        REQUIRE(ConverterTools::numberToRegisters<int32_t>(-2, false, false, 4) == std::vector<uint16_t>({0xffff, 0xffff, 0xffff, 0xfffe}));
    }

    SECTION("zero extend a positive value into the padding registers") {
        REQUIRE(ConverterTools::numberToRegisters<int32_t>(2, false, false, 4) == std::vector<uint16_t>({0x0000, 0x0000, 0x0000, 0x0002}));
        REQUIRE(ConverterTools::numberToRegisters<uint32_t>(2, false, false, 4) == std::vector<uint16_t>({0x0000, 0x0000, 0x0000, 0x0002}));
    }

    SECTION("return nothing when no registers are asked for") {
        REQUIRE(ConverterTools::numberToRegisters<int32_t>(1, false, false, 0).empty());
    }

    SECTION("round-trip every word and byte order") {
        const bool lowFirst = GENERATE(false, true);
        const bool swapBytes = GENERATE(false, true);

        const std::vector<uint16_t> registers(
            ConverterTools::numberToRegisters<uint64_t>(
                TestNumbers::Int64::ABCDEFGH_as_uint64, lowFirst, swapBytes, 4));

        REQUIRE(ConverterTools::registersToNumber<uint64_t>(registers, lowFirst, swapBytes) == TestNumbers::Int64::ABCDEFGH_as_uint64);
    }
}

TEST_CASE("ConverterTools floating point helpers should") {

    SECTION("read a double from four registers") {
        const std::vector<uint16_t> input({TestNumbers::Double::AB, TestNumbers::Double::CD,
                                           TestNumbers::Double::EF, TestNumbers::Double::GH});

        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<double>(input, false, false),
                     Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<double>(input, true, false),
                     Catch::Matchers::WithinULP(TestNumbers::Double::GHEFCDAB_as_double, 0));
    }

    SECTION("read a double with the bytes of every register swapped") {
        const std::vector<uint16_t> input({TestNumbers::Double::BA, TestNumbers::Double::DC,
                                           TestNumbers::Double::FE, TestNumbers::Double::HG});

        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<double>(input, false, true),
                     Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<double>(input, true, true),
                     Catch::Matchers::WithinULP(TestNumbers::Double::GHEFCDAB_as_double, 0));
    }

    SECTION("write a double to four registers") {
        const std::vector<uint16_t> expected({TestNumbers::Double::AB, TestNumbers::Double::CD,
                                              TestNumbers::Double::EF, TestNumbers::Double::GH});

        REQUIRE(ConverterTools::floatingPointToRegisters<double>(
                    TestNumbers::Double::ABCDEFGH_as_double, false, false, 4) == expected);
    }

    SECTION("read a float from two registers") {
        const std::vector<uint16_t> input({TestNumbers::Float::AB, TestNumbers::Float::CD});

        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<float>(input, false, false),
                     Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<float>(input, true, false),
                     Catch::Matchers::WithinULP(TestNumbers::Float::CDAB_as_float, 0));
    }

    SECTION("write a float to two registers") {
        REQUIRE(ConverterTools::floatingPointToRegisters<float>(
                    TestNumbers::Float::ABCD_as_float, false, false, 2) == std::vector<uint16_t>({TestNumbers::Float::AB, TestNumbers::Float::CD}));
        REQUIRE(ConverterTools::floatingPointToRegisters<float>(
                    TestNumbers::Float::ABCD_as_float, true, true, 2) == std::vector<uint16_t>({TestNumbers::Float::DC, TestNumbers::Float::BA}));
    }

    SECTION("preserve nan and infinity bit for bit") {
        const std::vector<uint16_t> nanRegisters(
            std::begin(TestNumbers::Double::NAN_REGISTERS), std::end(TestNumbers::Double::NAN_REGISTERS));
        const std::vector<uint16_t> posInfRegisters(
            std::begin(TestNumbers::Double::POS_INF_REGISTERS), std::end(TestNumbers::Double::POS_INF_REGISTERS));
        const std::vector<uint16_t> negInfRegisters(
            std::begin(TestNumbers::Double::NEG_INF_REGISTERS), std::end(TestNumbers::Double::NEG_INF_REGISTERS));

        REQUIRE(std::isnan(ConverterTools::registersToFloatingPoint<double>(nanRegisters, false, false)));

        const double posInf = ConverterTools::registersToFloatingPoint<double>(posInfRegisters, false, false);
        REQUIRE(std::isinf(posInf));
        REQUIRE(posInf > 0.0);

        const double negInf = ConverterTools::registersToFloatingPoint<double>(negInfRegisters, false, false);
        REQUIRE(std::isinf(negInf));
        REQUIRE(negInf < 0.0);

        REQUIRE(ConverterTools::floatingPointToRegisters<double>(posInf, false, false, 4) == posInfRegisters);
        REQUIRE(ConverterTools::floatingPointToRegisters<double>(negInf, false, false, 4) == negInfRegisters);
    }

    SECTION("round-trip a double through every word and byte order") {
        const bool lowFirst = GENERATE(false, true);
        const bool swapBytes = GENERATE(false, true);

        const std::vector<uint16_t> registers(
            ConverterTools::floatingPointToRegisters<double>(
                TestNumbers::Double::ABCDEFGH_as_double, lowFirst, swapBytes, 4));

        REQUIRE_THAT(ConverterTools::registersToFloatingPoint<double>(registers, lowFirst, swapBytes),
                     Catch::Matchers::WithinULP(TestNumbers::Double::ABCDEFGH_as_double, 0));
    }
}

TEST_CASE("ConverterTools 32-bit helpers should") {

    SECTION("read what registersToNumber reads") {
        const std::vector<uint16_t> input({TestNumbers::Int::AB, TestNumbers::Int::CD});

        REQUIRE(ConverterTools::registersToInt32(input, false, false) == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(ConverterTools::registersToInt32(input, true, false) == TestNumbers::Int::CDAB_as_int32);
        REQUIRE(ConverterTools::registersToInt32(input, false, true) == TestNumbers::Int::BADC_as_int32);
        REQUIRE(ConverterTools::registersToInt32(input, true, true) == TestNumbers::Int::DCBA_as_int32);
    }

    SECTION("write what numberToRegisters writes") {
        REQUIRE(ConverterTools::int32ToRegisters(TestNumbers::Int::ABCD_as_int32, false, false, 2) == std::vector<uint16_t>({TestNumbers::Int::AB, TestNumbers::Int::CD}));
        REQUIRE(ConverterTools::int32ToRegisters(TestNumbers::Int::ABCD_as_int32, true, false, 2) == std::vector<uint16_t>({TestNumbers::Int::CD, TestNumbers::Int::AB}));
        REQUIRE(ConverterTools::int32ToRegisters(TestNumbers::Int::ABCD_as_int32, false, false, 1) == std::vector<uint16_t>({TestNumbers::Int::CD}));
    }

    SECTION("reinterpret two registers as a 32-bit value of any type") {
        REQUIRE(ConverterTools::toNumber<int32_t>(TestNumbers::Int::AB, TestNumbers::Int::CD) == TestNumbers::Int::ABCD_as_int32);
        REQUIRE(ConverterTools::toNumber<uint32_t>(TestNumbers::Int::AB, TestNumbers::Int::CD) == TestNumbers::Int::ABCD_as_uint32);
        REQUIRE(ConverterTools::toNumber<int32_t>(TestNumbers::Int::BA, TestNumbers::Int::DC, true) == TestNumbers::Int::ABCD_as_int32);
        REQUIRE_THAT(ConverterTools::toNumber<float>(TestNumbers::Float::AB, TestNumbers::Float::CD),
                     Catch::Matchers::WithinULP(TestNumbers::Float::ABCD_as_float, 0));
    }
}

TEST_CASE("ConverterTools::setByteOrder should") {

    SECTION("swap the two bytes of a register only when asked") {
        REQUIRE(ConverterTools::setByteOrder(TestNumbers::Int::AB) == TestNumbers::Int::AB);
        REQUIRE(ConverterTools::setByteOrder(TestNumbers::Int::AB, false) == TestNumbers::Int::AB);
        REQUIRE(ConverterTools::setByteOrder(TestNumbers::Int::AB, true) == TestNumbers::Int::BA);
    }
}
