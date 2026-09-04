#pragma once

#include <cmath>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <iostream>
#include <stdexcept>
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
            INT64 = 3,
            UINT64 = 4
        } SourceType;

        static constexpr int NO_PRECISION = -1;

        static MqttValue fromInt(int32_t val) {
            return MqttValue(val);
        }

        static MqttValue fromInt64(int64_t val) {
            return MqttValue(val);
        }

        // No matching constructor on purpose: uint64_t is size_t on LP64, so a
        // MqttValue(uint64_t) would make MqttValue(someSizeT) compile there and
        // stay ambiguous on 32-bit targets.
        static MqttValue fromUInt64(uint64_t pVal) {
            MqttValue ret;
            ret.setUInt64(pVal);
            return ret;
        }

        static MqttValue fromDouble(double val, int precision = NO_PRECISION) {
            return MqttValue(val, precision);
        }

        static MqttValue fromBinary(const void* ptr, size_t size) {
            return MqttValue(ptr, size);
        }

        static MqttValue fromString(const std::string& pVal) {
            return MqttValue(pVal);
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

        MqttValue(const std::string& pVal) {
            setString(pVal.c_str());
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
            mType = SourceType::BINARY;
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

        void setUInt64(uint64_t pVal) {
            mValue.v_uint64 = pVal;
            mType = SourceType::UINT64;
        }

        void setBinary(const void* ptr, size_t size) {
            mBinaryValue = std::shared_ptr<void>(malloc(size), free);
            memcpy(mBinaryValue.get(), ptr, size);
            mBinarySize = size;
            mType = SourceType::BINARY;
        }

        std::string getString() const {
            switch (mType) {
            case SourceType::BINARY:
                return std::string(static_cast<const char*>(mBinaryValue.get()), mBinarySize);
            case SourceType::INT:
                return std::to_string(mValue.v_int);
            case SourceType::INT64:
                return std::to_string(mValue.v_int64);
            case SourceType::UINT64:
                return std::to_string(mValue.v_uint64);
            case SourceType::DOUBLE:
                return format(mValue.v_double);
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        double getDouble() const {
            switch (mType) {
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
            case SourceType::UINT64:
                return static_cast<double>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        int32_t getInt() const {
            switch (mType) {
            case SourceType::BINARY: {
                char* endptr;
                std::string strval(getString());
                int32_t ret = std::strtol(strval.c_str(), &endptr, 0);
                if (endptr == nullptr || *endptr != '\0') {
                    throw ConvException(std::string("Cannot convert ") + strval + " to int");
                }
                return ret;
            }
            case SourceType::INT:
                return mValue.v_int;
            case SourceType::INT64:
                return mValue.v_int64;
            case SourceType::UINT64:
                return static_cast<int32_t>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        uint16_t getUInt16() const {
            int32_t val = getInt();
            if (val < 0 || val > UINT16_MAX)
                throw ConvException(std::string("Conversion failed, value " + std::to_string(val) + " out of range"));
            return val;
        }

        int64_t getInt64() const {
            switch (mType) {
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
            case SourceType::UINT64:
                if (mValue.v_uint64 > static_cast<uint64_t>(INT64_MAX)) {
                    throw ConvException("Conversion failed, value " + std::to_string(mValue.v_uint64) + " out of int64 range");
                }
                return static_cast<int64_t>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        uint64_t getUInt64() const {
            switch (mType) {
            case SourceType::BINARY:
                return parseUInt64(getString());
            case SourceType::INT:
                return signedToUInt64(mValue.v_int);
            case SourceType::INT64:
                return signedToUInt64(mValue.v_int64);
            case SourceType::UINT64:
                return mValue.v_uint64;
            case SourceType::DOUBLE:
                if (!(mValue.v_double >= 0.0) || !(mValue.v_double < std::ldexp(1.0, 64))) {
                    throw ConvException("Conversion failed, value " + format(mValue.v_double) + " out of uint64 range");
                }
                return static_cast<uint64_t>(mValue.v_double);
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
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
            switch (mType) {
            case SourceType::BINARY:
                return mBinarySize;
            case SourceType::INT:
                return sizeof(int32_t);
            case SourceType::DOUBLE:
                return sizeof(double);
            case SourceType::INT64:
                return sizeof(int64_t);
            case SourceType::UINT64:
                return sizeof(uint64_t);
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        SourceType getSourceType() const { return mType; }
        int getDoublePrecision() const { return mDoublePrecision; }

    private:
        /**
         * Value holders
         * */
        typedef union {
            int64_t v_int64;
            // NOLINTNEXTLINE(readability-identifier-naming) - matches its siblings
            uint64_t v_uint64;
            int32_t v_int;
            double v_double;
        } Variant;

        Variant mValue;
        std::shared_ptr<void> mBinaryValue;
        size_t mBinarySize = 0;
        int mDoublePrecision = MqttValue::NO_PRECISION;
        SourceType mType;

        /**
         * Renders a whole value without a fractional part, anything else with
         * mDoublePrecision digits (stream default when no precision is set).
         *
         * The integral shortcuts are range-checked: converting a floating point
         * value that does not fit the target integer type is undefined, and a
         * whole value can easily exceed int64 (std.float32 reading 1e20, say).
         * Bounds are powers of two because those are exact in every floating
         * point format - (T)UINT64_MAX rounds up to 2^64 on a 53 bit mantissa
         * and would let an out-of-range value through.
         *
         * https://stackoverflow.com/questions/33125779/format-double-value-in-c
         */
        template <typename T>
        std::string format(T pValue) const {
            T intpart;
            const bool isWhole = (std::modf(pValue, &intpart) == T(0));

            if (isWhole && mDoublePrecision == NO_PRECISION) {
                if (intpart >= T(0) && intpart < std::ldexp(T(1), 64)) {
                    return std::to_string(static_cast<uint64_t>(intpart));
                }
                if (intpart < T(0) && intpart >= -std::ldexp(T(1), 63)) {
                    return std::to_string(static_cast<int64_t>(intpart));
                }
            }

            std::stringstream sstream;
            sstream.setf(std::ios::fixed);

            if (mDoublePrecision != NO_PRECISION) {
                sstream.precision(mDoublePrecision);
            } else if (isWhole) {
                // too large for the shortcuts above, but still has no fraction
                sstream.precision(0);
            }

            sstream << pValue;
            return sstream.str();
        }

        static uint64_t parseUInt64(const std::string& pStrval) {
            // strtoull silently wraps a negative value to a huge positive one
            const size_t first = pStrval.find_first_not_of(" \t");
            if (first == std::string::npos || pStrval[first] == '-') {
                throw ConvException("Cannot convert " + pStrval + " to uint64");
            }

            char* endptr;
            errno = 0;
            const uint64_t ret = std::strtoull(pStrval.c_str(), &endptr, 10);
            if (endptr == pStrval.c_str() || endptr == nullptr || *endptr != '\0' || errno == ERANGE) {
                throw ConvException("Cannot convert " + pStrval + " to uint64");
            }
            return ret;
        }

        template <typename T>
        static uint64_t signedToUInt64(T pVal) {
            if (pVal < 0) {
                throw ConvException("Conversion failed, value " + std::to_string(pVal) + " out of uint64 range");
            }
            return static_cast<uint64_t>(pVal);
        }
};
