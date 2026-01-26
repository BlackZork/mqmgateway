#pragma once

#include <iostream>

#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/convargs.hpp"

class DoubleRegisterArgTools {
    public:
        static bool getSwapBytes(const ConverterArgValues& values) {
            bool ret = false;
            const ConverterArgValue& val = values[ConverterArg::sSwapBytesArgName];
            try {
                ret = val.as_bool();
            } catch (const ConvException& ex) {
                std::string oldval = val.as_str();
                if (oldval == "")
                    ret = false;
                else if (oldval == "swap_bytes")
                    ret = true;
                else
                    throw;
                std::cerr << "[WARN ] swap_bytes param changed to bool, please update to 'swap_bytes=true'" << std::endl;
            }
            return ret;
        };

        static bool getLowFirst(const ConverterArgValues& values) {
            bool ret = false;
            const ConverterArgValue& val = values[ConverterArg::sLowFirstArgName];
            try {
                ret = val.as_bool();
            } catch (const ConvException& ex) {
                std::string oldval = val.as_str();
                if (oldval == "" || oldval == "high_first")
                    ret = false;
                else if (oldval == "low_first")
                    ret = true;
                else
                    throw;
                std::cerr << "[WARN ] low_first/high_first param changed to bool, please update to 'low_first=true|false'" << std::endl;
            }
            return ret;
        };
};
