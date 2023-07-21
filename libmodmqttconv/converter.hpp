#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "mqttvalue.hpp"
#include "modbusregisters.hpp"

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
           double val = std::stod(arg, nullptr);
           return val;
        }

        /**
         * Converts string argument to int
         * */
        static int toInt(const std::string& arg, int base = 10) {
           int val = std::stoi(arg, nullptr, base);
           return val;
        }

        /**
         * Returns nth converter argument as string
         * */
        static const std::string& getArg(int index, const std::vector<std::string>& args) {
            if (index < args.size())
                return args[index];
            throw std::out_of_range("Not enough arguments for converter");
        }

        /**
         * Returns nth converter argument as int
         * */
        static int getIntArg(int index, const std::vector<std::string>& args) {
            return toInt(getArg(index, args));
        }

        /**
         * Returns nth converter argument as double
         * */
        static double getDoubleArg(int index, const std::vector<std::string>& args) {
            return toDouble(getArg(index, args));
        }

        /**
         * Returns nth converter argument as uint16 mask
         * */
        static uint16_t getHex16Arg(int index, const std::vector<std::string>& args) {
            int ret = toInt(getArg(index, args), 16);
            if (ret < 0 || ret > 0xffff)
                throw std::out_of_range("value out of range");
            return (uint16_t)ret;
        }

        /**
         * Converts single or two registers to int32_t
         * */
        static int32_t registersToInt32(const ModbusRegisters& data, bool lowFirst) {
            int high = 0, low = 1;
            if (lowFirst) {
                high = 1;
                low = 0;
            }
            int32_t val = data.getValue(high);
            if (data.getCount() > 1) {
                val = val << 16;
                val += data.getValue(low);
            }
            return val;
        }

        /**
         * Converts int32 to single or two registers
         * */

        static ModbusRegisters int32ToRegisters(int32_t val, bool lowFirst, int registerCount) {
            ModbusRegisters ret;
            ret.appendValue(val);
            if (registerCount == 2) {
                if (!lowFirst)
                    ret.prependValue(val >> 16);
                else
                    ret.appendValue(val >> 16);
            }
            return ret;
        }

        /**
         * Swaps the low and high byte of a register, disregarding host endianness.
         *
         * @param value A register containing bytes A and B in order AB
         * @return A register containing bytes A and B in order BA
         */
        static uint16_t swapByteOrder(uint16_t value) {
            return ((value & 0x00ff) << 8) | ((value & 0xff00) >> 8);
        }

        /**
         * Swaps the low and high byte of each register, disregarding host endianness.
         *
         * @param value A list of registers
         */
        static void swapByteOrder(std::vector<uint16_t>& registers) {
            for (size_t i = 0; i < registers.size(); i++) {
                registers[i] = swapByteOrder(registers[i]);
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
            std::vector<uint16_t> registers({highRegister, lowRegister});
            if (swapBytes) {
                swapByteOrder(registers);
            }
            int32_t value = registersToInt32(ModbusRegisters(registers), false);
            return *reinterpret_cast<T*>(&value);
        }
};

class DataConverter {
    public:
        virtual void setArgs(const std::vector<std::string>& args) {};
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            throw std::logic_error("Converstion to mqtt value is not implemented");
        };
        virtual ModbusRegisters toModbus(const MqttValue&, int registerCount) const {
            throw std::logic_error("Converstion to modbus register values is not implemented");
        };
};

