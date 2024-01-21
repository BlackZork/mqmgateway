#pragma once

#include <cstring>
#include <memory>
#include <sstream>
#include <iostream>
#include "convexception.hpp"

class MqttValue {
    public:
        /**
         * Types of source for mqtt value
         * */
        typedef enum {
            INT = 0,
            DOUBLE = 1,
            BINARY = 2,
            INT64 = 3
        } SourceType;

        static constexpr int NO_PRECISION = -1;

        static MqttValue fromInt(int32_t val) {
            return MqttValue(val);
        }

        static MqttValue fromInt64(int64_t val) {
            return MqttValue(val);
        }

        static MqttValue fromDouble(double val, int precision = NO_PRECISION) {
            return MqttValue(val, precision);
        }

        static MqttValue fromBinary(const void* ptr, size_t size) {
            return MqttValue(ptr, size);
        }

        MqttValue() {
            setInt(0);
        }

        MqttValue(int32_t val) {
            setInt(val);
        }

        MqttValue(int64_t val) {
            setInt64(val);
        }

        MqttValue(double val, int precision = NO_PRECISION) {
            setDouble(val, precision);
        }

        MqttValue(const void* ptr, size_t size){
            mBinaryValue = std::shared_ptr<void>(malloc(size), free);
            memcpy(mBinaryValue.get(), ptr, size);
            mType = SourceType::BINARY;
            mBinarySize = size;
        }

        void setString(const char* val) {
            size_t len = strlen(val);
            mBinaryValue = std::shared_ptr<void>(malloc(len), free);
            memcpy(mBinaryValue.get(), val, len);
            mBinarySize = len;
        }

        void setDouble(double val, int precision) {
            mValue.v_double = val;
            mType = SourceType::DOUBLE;
            mDoublePrecision = precision;
        }

        void setInt(int32_t val) {
            mValue.v_int = val;
            mType = SourceType::INT;
        }

        void setInt64(int64_t val) {
            mValue.v_int64 = val;
            mType = SourceType::INT64;
        }

        void setBinary(const void* ptr, size_t size) {
            mBinaryValue = std::shared_ptr<void>(malloc(size), free);
            memcpy(mBinaryValue.get(), ptr, size);
            mType = SourceType::BINARY;
        }

        std::string getString() const {
            switch(mType) {
                case SourceType::BINARY:
                    return std::string(static_cast<const char*>(mBinaryValue.get()), mBinarySize);
                case SourceType::INT:
                    return std::to_string(mValue.v_int);
                case SourceType::INT64:
                    return std::to_string(mValue.v_int64);
                case SourceType::DOUBLE:
                    return format(mValue.v_double);
            }
            return std::string();
        }

        double getDouble() const {
            switch(mType) {
                case SourceType::BINARY: {
                    char* endptr;
                    std::string strval(getString());
                    double ret = std::strtod(strval.c_str(), &endptr);
                    if (endptr == nullptr || *endptr != '\0') {
                        throw ConvException(std::string("Cannot convert ") + strval + " to double");
                    }
                    return ret;
                }
                case SourceType::INT:
                    return mValue.v_int;
                case SourceType::INT64:
                    return mValue.v_int64;
                case SourceType::DOUBLE:
                    return mValue.v_double;
            }
            return 0;
        }

        int32_t getInt() const {
            switch(mType) {
                case SourceType::BINARY: {
                    char* endptr;
                    std::string strval(getString());
                    int32_t ret = std::strtol(strval.c_str(), &endptr, 10);
                    if (endptr == nullptr || *endptr != '\0') {
                        throw ConvException(std::string("Cannot convert ") + strval + " to int");
                    }
                    return ret;
                }
                case SourceType::INT:
                    return mValue.v_int;
                case SourceType::INT64:
                    return mValue.v_int64;
                case SourceType::DOUBLE:
                    return mValue.v_double;
            }
            return 0;
        }

        int64_t getInt64() const {
            switch(mType) {
                case SourceType::BINARY: {
                    char* endptr;
                    std::string strval(getString());
                    int64_t ret = std::strtoll(strval.c_str(), &endptr, 10);
                    if (endptr == nullptr || *endptr != '\0') {
                        throw ConvException(std::string("Cannot convert ") + strval + " to int64");
                    }
                    return ret;
                }
                case SourceType::INT:
                    return mValue.v_int;
                case SourceType::INT64:
                    return mValue.v_int64;
                case SourceType::DOUBLE:
                    return mValue.v_double;
            }
            return 0;
        }

        void* getBinaryPtr() const {
            switch(mType) {
                case SourceType::BINARY:
                    return mBinaryValue.get();
                default:
                    return (void*)&mValue;
            }
            return nullptr;
        }

        size_t getBinarySize() const {
            switch(mType) {
                case SourceType::BINARY:
                    return mBinarySize;
                case SourceType::INT:
                    return sizeof(int32_t);
                case SourceType::DOUBLE:
                    return sizeof(double);
                case SourceType::INT64:
                    return sizeof(int64_t);
            }
            return 0;
        }

        SourceType getSourceType() const { return mType; }
        int getDoublePrecision() const { return mDoublePrecision; }

    private:
        /**
         * Value holders
         * */
        typedef union {
            int64_t v_int64;
            int32_t v_int;
            double v_double;
        } Variant;

        Variant mValue;
        std::shared_ptr<void> mBinaryValue;
        size_t mBinarySize;
        int mDoublePrecision = -1;
        SourceType mType;

        // https://stackoverflow.com/questions/33125779/format-double-value-in-c
        std::string format(double value) const {

            double intpart;
            modf(value, &intpart);

            if (intpart == value && mDoublePrecision == NO_PRECISION)
                return std::to_string(int64_t(intpart));

            std::stringstream sstream;
            sstream.setf(std::ios::fixed);

            if (mDoublePrecision != -1)
                sstream.precision(mDoublePrecision);

            sstream << value;
            return sstream.str();
        }
};
