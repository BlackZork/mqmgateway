#pragma once


#include <map>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "convargs.hpp"
#include "mqttvalue.hpp"
#include "modbusregisters.hpp"

class DataConverter {
    public:
        /**
            Returns values set in yaml for converter instance by
            name defined in getArgs()

            Should throw ConvArgValidationException if arguments are invalid
        */
        virtual void setArgValues(const ConverterArgValues& values) {};

        /**
            Get a list of arguments that this converter needs for data conversion
        */
        virtual ConverterArgs getArgs() const { return ConverterArgs(); };

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            throw std::logic_error("Conversion to mqtt value is not implemented");
        };

        virtual ModbusRegisters toModbus(const MqttValue&, int registerCount) const {
            throw std::logic_error("Conversion to modbus register values is not implemented");
        };
};
