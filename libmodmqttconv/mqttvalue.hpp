#pragma once

#include <cstdint>
#include <cstring>
#include <variant>
#include <string>
#include <sstream>
#include <limits>

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

        MqttValue(int val) {
            setInt(val);
        }

        MqttValue(double val) {
            setDouble(val);
        }

        void setString(const char* val) {
            size_t len = strlen(val);
            mValue.binary = (char*)malloc(len);
            memcpy(mValue.binary, val, len);
        }
        void setDouble(double val) { mValue.v_double = val, mType = SourceType::DOUBLE; }
        void setInt(int32_t val) { mValue.v_int = val, mType = SourceType::INT; }

        std::string getString() const {
            switch(mType) {
                case SourceType::BINARY:
                    return std::string(static_cast<const char*>(mValue.binary), mBinarySize);
                case SourceType::INT:
                    return std::to_string(mValue.v_int);
                case SourceType::DOUBLE:
                    return double_to_string(mValue.v_double);
            }
            return std::string();
        }

        double getDouble() const {
            switch(mType) {
                case SourceType::BINARY:
                    return std::strtod(static_cast<const char*>(mValue.binary), nullptr);
                case SourceType::INT:
                    return mValue.v_int;
                case SourceType::DOUBLE:
                    return mValue.v_double;
            }
            return 0;
        }

        int32_t getInt() const {
            switch(mType) {
                case SourceType::BINARY:
                    return std::strtol(static_cast<const char*>(mValue.binary), nullptr, 10);
                case SourceType::INT:
                    return mValue.v_int;
                case SourceType::DOUBLE:
                    return mValue.v_double;
            }
            return 0;
        }

        void* getBinaryPtr() const {
            switch(mType) {
                case SourceType::BINARY:
                    return mValue.binary;
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

        ~MqttValue() {
            if (mType == SourceType::BINARY && mValue.binary != nullptr)
                free(mValue.binary);
        }
    private:
        /**
         * Value holders
         * */
        typedef union {
            int32_t v_int;
            double v_double;
            void* binary;
        } Variant;

        Variant mValue;
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
