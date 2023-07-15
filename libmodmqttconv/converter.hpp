#pragma once

#include <algorithm>
#include <cstdint>
#include <netinet/in.h>
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
         * Ensures that the bytes of each register are in host order.
         * */
        static void adaptToHostByteOrder(std::vector<uint16_t>& registers) {
            std::transform(registers.begin(), registers.end(), registers.begin(), ntohs);
        }
};

class DataConverter {
    public:
        virtual void setArgs(const std::vector<std::string>& args) {};
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            throw std::logic_error("Conversion to mqtt value is not implemented");
        };
        virtual ModbusRegisters toModbus(const MqttValue&, int registerCount) const {
            throw std::logic_error("Conversion to modbus register values is not implemented");
        };
};

