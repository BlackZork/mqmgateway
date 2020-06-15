#pragma once

#include <cstdint>

#include <vector>
#include <string>
#include <stdexcept>

#include "mqttvalue.hpp"
#include "modbusregisters.hpp"

/**
 *    Helper functions for IStateConverter interface.
 **/
class ConverterBase {
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
         * Returns nth converter argument as u_int16 mask
         * */
        static uint16_t getHex16Arg(int index, const std::vector<std::string>& args) {
            int ret = toInt(getArg(index, args), 16);
            if (ret < 0 || ret > 0xffff)
                throw std::out_of_range("value out of range");
            return (uint16_t)ret;
        }

        virtual void setArgs(const std::vector<std::string>& args) {};
        virtual ~ConverterBase() {};
};

class IStateConverter : public ConverterBase {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const = 0;
};
