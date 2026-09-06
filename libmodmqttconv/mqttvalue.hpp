#pragma once

#include <cmath>
#include <cerrno>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
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
            UINT64 = 4,
            FLOAT64 = 5
        } SourceType;

        static constexpr int NO_PRECISION = -1;

        /**
         * A floating point value that turned out to be a whole number small
         * enough for an integer type. There is no invalid state: a value with a
         * fractional part, one outside both integer ranges, or one that does not
         * come from a floating point source yields an empty optional instead.
         *
         * isSigned() selects which accessor is valid; taking the other one is a
         * usage error, not a conversion failure, so it throws std::logic_error.
         */
        class IntegralValue {
            public:
                static IntegralValue fromInt64(int64_t pVal) {
                    IntegralValue ret;
                    ret.mSigned = pVal;
                    ret.mIsSigned = true;
                    return ret;
                }

                static IntegralValue fromUInt64(uint64_t pVal) {
                    IntegralValue ret;
                    ret.mUnsigned = pVal;
                    return ret;
                }

                bool isSigned() const { return mIsSigned; }

                int64_t asInt64() const {
                    if (!mIsSigned) {
                        throw std::logic_error("IntegralValue does not hold a signed value");
                    }
                    return mSigned;
                }

                uint64_t asUInt64() const {
                    if (mIsSigned) {
                        throw std::logic_error("IntegralValue does not hold an unsigned value");
                    }
                    return mUnsigned;
                }

                /** Decimal text of whichever value is held. */
                std::string toString() const {
                    return mIsSigned ? std::to_string(asInt64()) : std::to_string(asUInt64());
                }

            private:
                IntegralValue() {}

                bool mIsSigned = false;
                // only the field mIsSigned selects is ever written or read
                union {
                        uint64_t mUnsigned = 0;
                        int64_t mSigned;
                };
        };

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

        // long double is the widest exact carrier an expression engine can hand
        // over: on amd64/i386 its 64 bit mantissa holds every uint64 exactly,
        // on arm64 113 bits do. On 32-bit arm it is just double, so a converter
        // needing exact 64 bit integers must use fromUInt64/fromInt64 instead.
        static MqttValue fromLongDouble(long double pVal, int pPrecision = NO_PRECISION) {
            MqttValue ret;
            ret.setLongDouble(pVal, pPrecision);
            return ret;
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

        void setLongDouble(long double pVal, int pPrecision) {
            mValue.v_ldouble = pVal;
            mType = SourceType::FLOAT64;
            mDoublePrecision = pPrecision;
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
            case SourceType::FLOAT64:
                return format(mValue.v_ldouble);
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
                // every uint64 is within double's range, only precision is lost
                return static_cast<double>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            case SourceType::FLOAT64:
                return longDoubleToDouble(mValue.v_ldouble);
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        /**
         * The widest exact carrier this platform has. On amd64, i386 and arm64
         * a long double mantissa holds every uint64, so a value that getDouble()
         * would round survives here. On 32-bit arm a long double is just a
         * double, so a caller needing exact 64 bit integers there must use
         * getInt64()/getUInt64() instead.
         *
         * The BINARY arm parses with strtold rather than strtod, which is what
         * makes it exact: a command payload reaches a converter as text, so the
         * rounding getDouble() suffers happens in the parse, not in a cast.
         */
        long double getLongDouble() const {
            switch (mType) {
            case SourceType::BINARY: {
                char* endptr;
                std::string strval(getString());
                long double ret = std::strtold(strval.c_str(), &endptr);
                if (endptr == nullptr || *endptr != '\0') {
                    throw ConvException(std::string("Cannot convert ") + strval + " to long double");
                }
                return ret;
            }
            case SourceType::INT:
                return mValue.v_int;
            case SourceType::INT64:
                return mValue.v_int64;
            case SourceType::UINT64:
                return static_cast<long double>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            case SourceType::FLOAT64:
                return mValue.v_ldouble;
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
                if (mValue.v_uint64 > static_cast<uint64_t>(INT32_MAX)) {
                    throw ConvException("Conversion failed, value " + std::to_string(mValue.v_uint64) + " out of int range");
                }
                return static_cast<int32_t>(mValue.v_uint64);
            case SourceType::DOUBLE:
                return mValue.v_double;
            case SourceType::FLOAT64:
                return floatToInt32(mValue.v_ldouble);
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
            case SourceType::FLOAT64:
                return floatToInt64(mValue.v_ldouble);
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
                return floatToUInt64(mValue.v_double);
            case SourceType::FLOAT64:
                return floatToUInt64(mValue.v_ldouble);
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
            case SourceType::FLOAT64:
                return sizeof(long double);
            }
            throw std::logic_error("Unhandled MqttValue source type " + std::to_string(mType));
        }

        /**
         * Reports a floating point value that has no fractional part and fits
         * an integer type, so that it can be rendered or serialized without
         * going through a double. pIsSigned selects which output holds it.
         * False for a fractional value, a value outside both ranges, or a
         * source type that is not floating point.
         */
        std::optional<IntegralValue> asIntegral() const {
            switch (mType) {
            case SourceType::DOUBLE:
                return toIntegral(mValue.v_double);
            case SourceType::FLOAT64:
                return toIntegral(mValue.v_ldouble);
            case SourceType::BINARY:
            case SourceType::INT:
            case SourceType::INT64:
            case SourceType::UINT64:
                return std::nullopt;
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
            // NOLINTNEXTLINE(readability-identifier-naming) - matches its siblings
            long double v_ldouble;
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
                const std::optional<IntegralValue> integral = wholeToIntegral(intpart);
                if (integral.has_value()) {
                    return integral->toString();
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

        /**
         * Splits a whole floating point value into the widest integer type that
         * holds it exactly. Bounds are powers of two because those are exact in
         * every floating point format - (T)UINT64_MAX rounds up to 2^64 on a 53
         * bit mantissa and would let an out-of-range value through, and an
         * out-of-range float to integer conversion is undefined.
         */
        template <typename T>
        static std::optional<IntegralValue> toIntegral(T pValue) {
            T intpart;
            // a fractional value is ordinary data here, not a usage error
            if (std::modf(pValue, &intpart) != T(0)) {
                return std::nullopt;
            }
            return wholeToIntegral(intpart);
        }

        /**
         * Range half of toIntegral, for callers that already know the value has
         * no fractional part and have the whole part at hand. The sign decides
         * the target type, so only one bound needs testing in either branch.
         */
        template <typename T>
        static std::optional<IntegralValue> wholeToIntegral(T pIntpart) {
            if (pIntpart < T(0)) {
                if (pIntpart >= -std::ldexp(T(1), 63)) {
                    return IntegralValue::fromInt64(static_cast<int64_t>(pIntpart));
                }
            } else if (pIntpart < std::ldexp(T(1), 64)) {
                return IntegralValue::fromUInt64(static_cast<uint64_t>(pIntpart));
            }
            return std::nullopt;
        }

        /**
         * Range-checked narrowing of a floating point value. Converting one
         * that does not fit the target integer type is undefined, so each of
         * these throws instead, the way getUInt16() already does. The !(a >= b)
         * spelling also rejects NaN, which compares false against everything.
         */
        template <typename T>
        static uint64_t floatToUInt64(T pValue) {
            if (!(pValue >= T(0)) || !(pValue < std::ldexp(T(1), 64))) {
                throw ConvException("Conversion failed, value " + plainText(pValue) + " out of uint64 range");
            }
            return static_cast<uint64_t>(pValue);
        }

        template <typename T>
        static int64_t floatToInt64(T pValue) {
            if (!(pValue >= -std::ldexp(T(1), 63)) || !(pValue < std::ldexp(T(1), 63))) {
                throw ConvException("Conversion failed, value " + plainText(pValue) + " out of int64 range");
            }
            return static_cast<int64_t>(pValue);
        }

        template <typename T>
        static int32_t floatToInt32(T pValue) {
            if (!(pValue >= T(INT32_MIN)) || !(pValue <= T(INT32_MAX))) {
                throw ConvException("Conversion failed, value " + plainText(pValue) + " out of int range");
            }
            return static_cast<int32_t>(pValue);
        }

        /**
         * long double reaches ~1.2e4932 where double stops at ~1.8e308, so the
         * narrowing overflows for values a FLOAT64 can legitimately hold.
         * Infinities and NaN pass through, they exist in double too.
         */
        static double longDoubleToDouble(long double pValue) {
            if (std::isfinite(pValue) && (pValue > static_cast<long double>(DBL_MAX) || pValue < -static_cast<long double>(DBL_MAX))) {
                throw ConvException("Conversion failed, value " + plainText(pValue) + " out of double range");
            }
            return static_cast<double>(pValue);
        }

        template <typename T>
        static std::string plainText(T pValue) {
            std::stringstream sstream;
            sstream << pValue;
            return sstream.str();
        }

        template <typename T>
        static uint64_t signedToUInt64(T pVal) {
            if (pVal < 0) {
                throw ConvException("Conversion failed, value " + std::to_string(pVal) + " out of uint64 range");
            }
            return static_cast<uint64_t>(pVal);
        }
};
