#pragma once

#include <vector>
#include <string>
#include <stdexcept>

#include "mqttvalue.hpp"
#include "modbusregisters.hpp"

class ConverterBase {
    public:
        static double toDouble(const std::string& arg) {
           double val = std::stod(arg, nullptr);
           return val;
        }

        static int toInt(const std::string& arg) {
           int val = std::stoi(arg, nullptr);
           return val;
        }

        static const std::string& getArg(int index, const std::vector<std::string> args) {
            if (index < args.size())
                return args[index];
            throw std::out_of_range("Not enough arguments for converter");
        }

        static int getIntArg(int index, const std::vector<std::string> args) {
            return toInt(getArg(index, args));
        }

        static int getDoubleArg(int index, const std::vector<std::string> args) {
            return toDouble(getArg(index, args));
        }

        virtual void setArgs(const std::vector<std::string>& args) {};
        virtual ~ConverterBase() {};
};

class IStateConverter : public ConverterBase {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& data) const = 0;
};
