#pragma once

#include <cstdint>
#include <cstring>
#include <variant>
#include <string>
#include <sstream>
#include <limits>
#include <memory>

#include "convexception.hpp"

class MqttValue {
    public:
        /**
         * Types of source for mqtt value
         * */
        typedef enum {
            INT = 0,
            DOUBLE = 1,
            BINARY = 2
        } SourceType;

        static MqttValue fromInt(int val) {
            return MqttValue(val);
        }

        static MqttValue fromDouble(double val) {
            return MqttValue(val);
        }

        static MqttValue fromBinary(const void* ptr, size_t size) {
            return MqttValue(ptr, size);
        }

        MqttValue() {
            setInt(0);
        }

        MqttValue(int val) {
            setInt(val);
        }

        MqttValue(double val) {
            setDouble(val);
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

        void setDouble(double val) {
            mValue.v_double = val;
            mType = SourceType::DOUBLE;
        }

        void setInt(int32_t val) {
            mValue.v_int = val;
            mType = SourceType::INT;
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
                case SourceType::DOUBLE:
                    return double_to_string(mValue.v_double);
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
                        throw ConvException(std::string("Cannot convert") + strval + " to double");
                    }
                    return ret;
                }
                case SourceType::INT:
                    return mValue.v_int;
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
                        throw ConvException(std::string("Cannot convert") + strval + " to int");
                    }
                    return ret;
                } case SourceType::INT:
                    return mValue.v_int;
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
            }
            return 0;
        }

        SourceType getSourceType() const { return mType; }

    private:
        /**
         * Value holders
         * */
        typedef union {
            int32_t v_int;
            double v_double;
        } Variant;

        Variant mValue;
        std::shared_ptr<void> mBinaryValue;
        size_t mBinarySize;
        SourceType mType;

        //https://codereview.stackexchange.com/questions/90565/converting-a-double-to-a-stdstring-without-scientific-notation
        static std::string double_to_string(double d)
        {
            std::string str = std::to_string(d);

            // Remove padding
            // This must be done in two steps because of numbers like 700.00
            std::size_t pos1 = str.find_last_not_of("0");
            if(pos1 != std::string::npos)
                str.erase(pos1+1);

            std::size_t pos2 = str.find_last_not_of(".");
            if(pos2 != std::string::npos)
                str.erase(pos2+1);

            return str;
        }
};
