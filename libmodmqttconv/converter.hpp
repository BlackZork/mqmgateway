#pragma once


#include <map>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "convargs.hpp"
#include "convexception.hpp"
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

        /**
            How many modbus registers this converter is designed for, or 0 if it
            works with any number of them.

            modmqttd warns when a configured register count differs from this.
            Whether a mismatch is fatal is up to the converter: std.int32 reads
            whatever it is given, std.float32 refuses anything but two registers.
        */
        virtual int getExpectedRegisterCount() const { return 0; }

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            throw std::logic_error("Conversion to mqtt value is not implemented");
        };

        virtual ModbusRegisters toModbus(const MqttValue&, int registerCount) const {
            throw std::logic_error("Conversion to modbus register values is not implemented");
        };

    protected:
        /**
            Throws unless pCount matches getExpectedRegisterCount(), for a
            converter whose width is not negotiable - a float needs exactly its
            own bit pattern, no more and no fewer registers.

            pValueName names what is being converted, so the message reads
            "64-bit float needs 4 registers, got 2".
        */
        void requireExpectedRegisterCount(int pCount, const std::string& pValueName) const {
            if (pCount != getExpectedRegisterCount()) {
                throw ConvException(pValueName + " needs " + std::to_string(getExpectedRegisterCount()) + " registers, got " + std::to_string(pCount));
            }
        }
};
