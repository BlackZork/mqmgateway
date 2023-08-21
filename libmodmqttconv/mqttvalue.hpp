#pragma once

#include <cstring>
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
            BINARY = 2,
            INT64 = 3
        } SourceType;

        static MqttValue fromInt(int32_t val) {
            return MqttValue(val);
        }

        static MqttValue fromInt64(int64_t val) {
            return MqttValue(val);
        }

        static MqttValue fromDouble(double val, int precision) {
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

        MqttValue(double val, int precision) {
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
            mValue.v_double = precision < 0 ? val : round(val, precision);
            mType = SourceType::DOUBLE;
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
                        throw ConvException(std::string("Cannot convert") + strval + " to double");
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
                        throw ConvException(std::string("Cannot convert") + strval + " to int");
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
                        throw ConvException(std::string("Cannot convert") + strval + " to int64");
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
        SourceType mType;

        /**
         * round double value to decimal_digits for
         * string formatting
         */
        static double round(double value, int precision) {
            double divisor = pow(10, precision);
            return std::round(value * divisor) / divisor;
        }

        //https://codereview.stackexchange.com/questions/90565/converting-a-double-to-a-stdstring-without-scientific-notation
        static std::string format(double value) {
            std::string str = std::to_string(value);

            // Remove padding
            // This must be done in two steps because of numbers like 700.00
            if (str.find('.') != std::string::npos) {
                removeTrailing('0', str);
                removeTrailing('.', str);
            }
            return str;
        }

       static void removeTrailing(char c, std::string& str) {
            size_t pos = str.find_last_not_of(c);
            if (pos != std::string::npos) {
                str.erase(pos + 1);
            }
       }

};
