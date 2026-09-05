#pragma once

#include <algorithm>
#include <cassert>
#include <netinet/in.h>
#include <cstdint>
#include <cstring>

#include <string>
#include <stdexcept>
#include <type_traits>
#include <vector>

using namespace std::string_literals;

/**
 *    Helper functions for DataConverter interface.
 **/
class ConverterTools {
    private:
        ConverterTools() {};
    public:
        /**
         * Converts string argument to double
         * */
        static double toDouble(const std::string& arg) {
            std::size_t pos;
            double val = std::stod(arg, &pos);
            if (pos != arg.length())
                throw std::invalid_argument(arg + " has unparsable chars starting from idx=" + std::to_string(pos) + " when converting to double");
            return val;
        }


        /**
         * Converts string argument to int
         * */
        static int toInt(const std::string& arg, int base = 10) {
            std::size_t pos;
            int val = std::stoi(arg, &pos, base);
            if (pos != arg.length())
                throw std::invalid_argument(arg + " has unparsable chars starting from idx=" + std::to_string(pos) + " when converting to int");
            return val;
        }

        /**
         * Combines registers into a number of type T. The first register holds the
         * most significant word unless pLowFirst is set, in which case the whole
         * word order is reversed (R0 R1 R2 R3 is read as R3 R2 R1 R0).
         *
         * Only the first sizeof(T)/2 registers are used; when fewer are given the
         * value is built from those alone and is zero extended, never sign extended.
         *
         * @param pSwapBytes If set, the two bytes of every register are swapped
         *                   before the registers are combined.
         * */
        template <typename T>
        static T registersToNumber(const std::vector<uint16_t>& pRegisters, bool pLowFirst, bool pSwapBytes) {
            static_assert(std::is_integral<T>::value, "registersToNumber() needs an integral type");
            static_assert(sizeof(T) >= sizeof(uint16_t), "registersToNumber() needs a type at least one register wide");
            static_assert(sizeof(T) <= sMaxWordCount * sizeof(uint16_t), "registersToNumber() handles at most 64 bits");

            typedef typename std::make_unsigned<T>::type UnsignedType;
            const uint64_t bits = registersToBits(pRegisters, pLowFirst, pSwapBytes, sizeof(T) / sizeof(uint16_t));
            return static_cast<T>(static_cast<UnsignedType>(bits));
        }

        /**
         * Splits a number of type T into exactly pRegisterCount registers. The first
         * register holds the most significant word unless pLowFirst is set, in which
         * case the whole word order is reversed.
         *
         * A count smaller than the width of T keeps the least significant words. A
         * count larger than 64 bits pads with sign extension for a negative value and
         * with zeroes otherwise.
         *
         * @param pSwapBytes If set, the two bytes of every register are swapped after
         *                   the value is split.
         * */
        template <typename T>
        static std::vector<uint16_t> numberToRegisters(T pValue, bool pLowFirst, bool pSwapBytes, int pRegisterCount) {
            static_assert(std::is_integral<T>::value, "numberToRegisters() needs an integral type");
            static_assert(sizeof(T) <= sMaxWordCount * sizeof(uint16_t), "numberToRegisters() handles at most 64 bits");

            uint64_t bits;
            uint16_t fill;
            if constexpr (std::is_signed<T>::value) {
                // widening through int64_t is what makes the padding words carry the sign
                bits = static_cast<uint64_t>(static_cast<int64_t>(pValue));
                fill = pValue < 0 ? 0xffff : 0x0000;
            } else {
                bits = static_cast<uint64_t>(pValue);
                fill = 0x0000;
            }
            return bitsToRegisters(bits, fill, pLowFirst, pSwapBytes, pRegisterCount);
        }

        /**
         * Reads the IEEE 754 bit pattern held in the registers as a floating point
         * value. Word and byte order are handled as in registersToNumber().
         * */
        template <typename T>
        static T registersToFloatingPoint(const std::vector<uint16_t>& pRegisters, bool pLowFirst, bool pSwapBytes) {
            static_assert(std::is_floating_point<T>::value, "registersToFloatingPoint() needs a floating point type");
            static_assert(sizeof(T) == sizeof(BitsOf<T>), "registersToFloatingPoint() handles 32 and 64 bit floating point types only");

            const BitsOf<T> bits = registersToNumber<BitsOf<T>>(pRegisters, pLowFirst, pSwapBytes);
            T ret;
            std::memcpy(&ret, &bits, sizeof(ret));
            return ret;
        }

        /**
         * Writes the IEEE 754 bit pattern of a floating point value to registers.
         * Word and byte order are handled as in numberToRegisters().
         * */
        template <typename T>
        static std::vector<uint16_t> floatingPointToRegisters(T pValue, bool pLowFirst, bool pSwapBytes, int pRegisterCount) {
            static_assert(std::is_floating_point<T>::value, "floatingPointToRegisters() needs a floating point type");
            static_assert(sizeof(T) == sizeof(BitsOf<T>), "floatingPointToRegisters() handles 32 and 64 bit floating point types only");

            BitsOf<T> bits;
            std::memcpy(&bits, &pValue, sizeof(bits));
            return numberToRegisters<BitsOf<T>>(bits, pLowFirst, pSwapBytes, pRegisterCount);
        }

        /**
         * Converts single or two registers to int32_t
         * */
        static int32_t registersToInt32(const std::vector<uint16_t>& data, bool lowFirst, bool swapBytes) {
            return registersToNumber<int32_t>(data, lowFirst, swapBytes);
        }

        /**
         * Converts int32 to single or two registers
         * */

        static std::vector<uint16_t> int32ToRegisters(int32_t val, bool lowFirst, bool swapBytes, int registerCount) {
            return numberToRegisters<int32_t>(val, lowFirst, swapBytes, registerCount);
        }

        /**
         * Ensures that the bytes of each register are in network order.
         * */
        static void adaptToNetworkByteOrder(std::vector<uint16_t>& registers) {
            for (size_t i = 0; i < registers.size(); i++) {
                registers[i] = htons(registers[i]);
            }
        }

        /**
         * Swaps the low and high byte of a register, disregarding host endianness.
         *
         * @param value A register containing bytes A and B in order AB
         * @return A register containing bytes A and B in order BA
         */
        static uint16_t setByteOrder(uint16_t value, bool swap = false) {
            if (!swap)
                return value;
            return ((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
        }

        /**
         * Swaps the low and high byte of each register, disregarding host endianness.
         *
         * @param value A list of registers
         */
        static void swapByteOrder(std::vector<uint16_t>& registers) {
            for (size_t i = 0; i < registers.size(); i++) {
                registers[i] = setByteOrder(registers[i], true);
            }
        }

        /**
         * Converts two registers (e.g. r1=0xA1B2 and r2=0xC3D4) to one 32-bit number (e.g. n=0xA1B2C3D4).
         *
         * @tparam T Type of the 32-bit number
         * @param highRegister A register containing the most significant bytes (e.g. 0xA1B2)
         * @param lowRegister  A register containing the least significant bytes (e.g. 0xC3D4)
         * @param swapBytes    If set to true, the high and low byte of both registers are swapped
         *                     (e.g. n=0xB2A1D4C3).
         * @return A number containing the bytes of both registers
         */
        template <typename T>
        static T toNumber(const uint16_t highRegister, const uint16_t lowRegister, const bool swapBytes = false) {
            static_assert(sizeof(T) == sizeof(uint32_t), "toNumber() handles 32-bit types only");

            const uint32_t bits = static_cast<uint32_t>(
                registersToBits({highRegister, lowRegister}, false, swapBytes, 2));
            T ret;
            std::memcpy(&ret, &bits, sizeof(ret));
            return ret;
        }

    private:
        /** The widest value the register helpers can carry, in registers. */
        static constexpr size_t sMaxWordCount = 4;

        /** The unsigned integer type holding the bit pattern of a type of the same width. */
        template <typename T>
        using BitsOf = typename std::conditional<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>::type;

        /**
         * Combines up to pWordCount registers into the low bits of an uint64_t, most
         * significant register first unless pLowFirst is set.
         */
        static uint64_t registersToBits(const std::vector<uint16_t>& pRegisters, bool pLowFirst, bool pSwapBytes, size_t pWordCount) {
            const size_t count = std::min(pRegisters.size(), pWordCount);
            uint64_t ret = 0;
            for (size_t i = 0; i < count; i++) {
                const size_t idx = pLowFirst ? (count - 1 - i) : i;
                ret = (ret << 16) | setByteOrder(pRegisters[idx], pSwapBytes);
            }
            return ret;
        }

        /**
         * Splits the low bits of pBits into pRegisterCount registers, most significant
         * first unless pLowFirst is set. Registers above what pBits can hold get pFill.
         */
        static std::vector<uint16_t> bitsToRegisters(uint64_t pBits, uint16_t pFill, bool pLowFirst, bool pSwapBytes, int pRegisterCount) {
            std::vector<uint16_t> ret;
            if (pRegisterCount <= 0) {
                return ret;
            }
            ret.reserve(pRegisterCount);
            for (int i = pRegisterCount - 1; i >= 0; i--) {
                const uint16_t word = i < static_cast<int>(sMaxWordCount)
                                          ? static_cast<uint16_t>(pBits >> (16 * i))
                                          : pFill;
                ret.push_back(setByteOrder(word, pSwapBytes));
            }
            if (pLowFirst) {
                std::reverse(ret.begin(), ret.end());
            }
            return ret;
        }
};
