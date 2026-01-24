#pragma once

#include "libmodmqttconv/converter.hpp"

class DoubleRegisterConverter : public DataConverter {
    protected:
        void setSwapBytes(const ConverterArgValues& values) {
            const ConverterArgValue& val = values[ConverterArg::sSwapBytesArgName];
            try {
                mSwapBytes = val.as_bool();
            } catch (const ConvException& ex) {
                std::string oldval = val.as_str();
                if (oldval == "")
                    mSwapBytes = false;
                else if (oldval == "swap_bytes")
                    mSwapBytes = true;
                else
                    throw;
                std::cerr << "swap_bytes param changed to bool, please update to 'swap_bytes=true'" << std::endl;
            }
        }

        void setLowFirst(const ConverterArgValues& values) {
            const ConverterArgValue& val = values[ConverterArg::sLowFirstArgName];
            try {
                mLowFirst = val.as_bool();
            } catch (const ConvException& ex) {
                std::string oldval = val.as_str();
                if (oldval == "")
                    mLowFirst = false;
                else if (oldval == "low_first")
                    mLowFirst = true;
                else
                    throw;
                std::cerr << "low_first param changed to bool, please update to 'low_first=true'" << std::endl;
            }
        }

        bool mLowFirst;
        bool mSwapBytes;
};
