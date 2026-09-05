#pragma once

#include <exprtk.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

#include "int64support.hpp"

static std::string
strvecToString(const std::vector<std::string>& pElements, const char* const pDelimiter) {
    std::ostringstream os;
    auto b = begin(pElements), e = end(pElements);

    if (b != e) {
        std::copy(b, prev(e), std::ostream_iterator<std::string>(os, pDelimiter));
        b = prev(e);
    }
    if (b != e) {
        os << *b;
    }

    return os.str();
}

class ExprtkConverter : public DataConverter {
    public:
        static const int MAX_REGISTERS = 10;

        ExprtkConverter() : mValues(MAX_REGISTERS, 0) {}

        virtual MqttValue toMqtt(const ModbusRegisters& data) const {

            if (data.getCount() > MAX_REGISTERS)
                throw ConvException("Maximum " +std::to_string(MAX_REGISTERS) + " registers allowed");

            for(int i = 0; i < data.getCount(); i++) {
                mValues[i] =  data.getValue(i);
            }
            // an expression may name any of R0..R9, so clear the ones this poll
            // did not supply instead of leaving the previous poll's values there
            for (int i = data.getCount(); i < MAX_REGISTERS; i++) {
                mValues[i] = 0;
            }

            long double ret = mExpression.value();

            return MqttValue::fromLongDouble(ret, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;

            mValues[0] = value.getLongDouble();

            long double exprval = mExpression.value();

            typedef exprtk::results_context<long double> ResultsContext;
            const auto& results = mExpression.results();

            if (std::isnan(exprval)) {
                if (results.count() > 100)
                    throw ConvException("Too many values returned, max=100");

                int registersNeeded = getWriteRegistersCount((int)results.count());

                if (registersNeeded != registerCount)
                    throw ConvException("Got " + std::to_string(results.count()) + " values, need " + std::to_string(registerCount));

                typedef typename ResultsContext::type_store_t TypeStore;
                typedef typename TypeStore::scalar_view ScalarView;

                for(std::size_t i = 0; i < results.count(); i++) {
                    TypeStore ts = results[i];
                    switch (ts.type) {
                    case TypeStore::e_scalar: {
                        long double val = ScalarView(ts)();
                        writeRegisterValues(ret, val);
                    } break;
                    case TypeStore::e_vector:
                        throw ConvException("Invalid list returned on position " + std::to_string(i));
                    case TypeStore::e_string:
                        throw ConvException("Invalid string value returned on position " + std::to_string(i));
                    case TypeStore::e_unknown:
                        throw ConvException("Unknown value type returned on position " + std::to_string(i));
                    }
                }
            } else {
                int registersNeeded = getWriteRegistersCount(1);
                if (registerCount != registersNeeded)
                    throw ConvException("Got a single value, need " + std::to_string(registersNeeded));

                writeRegisterValues(ret, exprval);
            }

            return ret;
        };


        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("expression", ConverterArgType::STRING, "");
            ret.add(ConverterArg::sPrecisionArgName, ConverterArgType::INT, ConverterArgValue::NO_PRECISION);
            ret.add("write_as", ConverterArgType::STRING, "");
            ret.add(ConverterArg::sLowFirstArgName, ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& values) {
            mSymbolTable.add_function("int32",   int32);
            mSymbolTable.add_function("int32bs",   int32bs);
            mSymbolTable.add_function("uint32",  uint32);
            mSymbolTable.add_function("uint32bs",  uint32bs);
            mSymbolTable.add_function("flt32",   flt32);
            mSymbolTable.add_function("flt32bs", flt32bs);
            mSymbolTable.add_function("int16", int16);
            mSymbolTable.add_function("int16bs", int16bs);
            mSymbolTable.add_function("uint16bs", uint16bs);
            mSymbolTable.add_function("flt64", flt64);
            mSymbolTable.add_function("flt64bs", flt64bs);
            // only the integer helpers need a mantissa wide enough to hold every
            // bit; a double read through flt64 is exact at any long double width
            if constexpr (exprconv::sExactInt64) {
                mSymbolTable.add_function("int64", int64);
                mSymbolTable.add_function("int64bs", int64bs);
                mSymbolTable.add_function("uint64", uint64);
                mSymbolTable.add_function("uint64bs", uint64bs);
            }
            mSymbolTable.add_constants();

            char buf[16];
            for(uint16_t i = 0; i < mValues.size(); i++) {
                sprintf(buf, "R%d", i);
                mSymbolTable.add_variable(buf, mValues[i], false);

                sprintf(buf, "M%d", i);
                mSymbolTable.add_variable(buf, mValues[0], false);
            }


            mExpression.register_symbol_table(mSymbolTable);
            if (!mParser.compile(values["expression"].as_str(), mExpression)) {
                std::string error = std::string("Exprtk ") + mParser.error();
                const std::optional<std::string> hint = exprconv::narrowLongDoubleHint(values["expression"].as_str());
                if (hint.has_value()) {
                    error += ". " + *hint;
                }
                throw ConvException(error);
            }

            mPrecision = values[ConverterArg::sPrecisionArgName].as_int();
            // for write only
            mWriteAs = values["write_as"].as_str();
            mWriteHelper = findWriteHelper(mWriteAs.empty() ? sDefaultWriteHelper : mWriteAs);

            if (mWriteHelper == writeHelpers().end()) {
                // a gated helper is a real name that this platform cannot offer,
                // which is a different thing to say than "unknown"
                const std::optional<std::string> hint = exprconv::narrowLongDoubleHint(mWriteAs);
                if (hint.has_value()) {
                    throw ConvException("Cannot write as "s + mWriteAs + ": " + *hint);
                }
                throw ConvException("Unknown write helper "s + mWriteAs + ", valid helpers:" + availableWriteHelpers());
            }

            mWriteLowFirst = values[ConverterArg::sLowFirstArgName].as_bool();
        }

        virtual ~ExprtkConverter() {
            mExpression.release();
        }
    private:
        /**
         * How a write_as name turns into registers. One table so that the set of
         * valid names, the register count each needs and the writer each uses
         * cannot drift apart, which is what let a duplicated uint16bs branch sit
         * here wired to the wrong writer.
         */
        struct WriteHelper {
                const char* mName;
                int mRegisterCount;
                bool mSwapBytes;
                bool mNeedsExactInt64;
                void (ExprtkConverter::*mWriter)(ModbusRegisters&, long double, bool) const;
        };

        typedef std::vector<WriteHelper>::const_iterator WriteHelperIterator;

        /** What an unset write_as means, so that it needs no case of its own. */
        static constexpr const char* sDefaultWriteHelper = "uint16";

        exprtk::symbol_table<long double> mSymbolTable;
        exprtk::parser<long double> mParser;
        exprtk::expression<long double> mExpression;
        mutable std::vector<long double> mValues;
        int mPrecision = ConverterArgValue::NO_PRECISION;
        std::string mWriteAs;
        WriteHelperIterator mWriteHelper = findWriteHelper(sDefaultWriteHelper);
        bool mWriteLowFirst = false;

        static long double int32(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<int32_t>(pHighRegister, pLowRegister, false);
        }

        static long double int32bs(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<int32_t>(pHighRegister, pLowRegister, true);
        }

        static long double uint32(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<uint32_t>(pHighRegister, pLowRegister, false);
        }

        static long double uint32bs(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<uint32_t>(pHighRegister, pLowRegister, true);
        }

        static long double flt32(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<float>(pHighRegister, pLowRegister, false);
        }

        static long double flt32bs(const long double pHighRegister, const long double pLowRegister) {
            return ConverterTools::toNumber<float>(pHighRegister, pLowRegister, true);
        }

        /**
         * The four registers of a 64-bit value, most significant word first.
         * A caller wanting the reverse word order passes them reversed, the way
         * the 32-bit helpers already take int32(R1, R0).
         */
        static std::vector<uint16_t> toRegisters(const long double pWord0, const long double pWord1, const long double pWord2,
                                                 const long double pWord3) {
            return {static_cast<uint16_t>(pWord0), static_cast<uint16_t>(pWord1), static_cast<uint16_t>(pWord2),
                    static_cast<uint16_t>(pWord3)};
        }

        static long double int64(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return static_cast<long double>(ConverterTools::registersToNumber<int64_t>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, false));
        }

        static long double int64bs(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return static_cast<long double>(ConverterTools::registersToNumber<int64_t>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, true));
        }

        static long double uint64(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return static_cast<long double>(ConverterTools::registersToNumber<uint64_t>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, false));
        }

        static long double uint64bs(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return static_cast<long double>(ConverterTools::registersToNumber<uint64_t>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, true));
        }

        static long double flt64(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return ConverterTools::registersToFloatingPoint<double>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, false);
        }

        static long double flt64bs(const long double pWord0, const long double pWord1, const long double pWord2, const long double pWord3) {
            return ConverterTools::registersToFloatingPoint<double>(toRegisters(pWord0, pWord1, pWord2, pWord3), false, true);
        }

        static long double int16(const long double pRegValue) {
            uint16_t val = uint16_t(pRegValue);
            return (int16_t)val;
        }

        static long double uint16bs(const long double pRegValue) {
            uint16_t val = uint16_t(pRegValue);
            val = ConverterTools::setByteOrder(val, true);
            return val;
        }

        static long double int16bs(const long double pRegValue) {
            uint16_t val = uint16_t(pRegValue);
            val = ConverterTools::setByteOrder(val, true);
            return (int16_t)(val);
        }


        static const std::vector<WriteHelper>& writeHelpers() {
            static const std::vector<WriteHelper> helpers{
                {"uint16", 1, false, false, &ExprtkConverter::writeUInt16},
                {"uint16bs", 1, true, false, &ExprtkConverter::writeUInt16},
                {"int16", 1, false, false, &ExprtkConverter::writeInt16},
                {"int16bs", 1, true, false, &ExprtkConverter::writeInt16},
                {"uint32", 2, false, false, &ExprtkConverter::writeUInt32},
                {"uint32bs", 2, true, false, &ExprtkConverter::writeUInt32},
                {"int32", 2, false, false, &ExprtkConverter::writeInt32},
                {"int32bs", 2, true, false, &ExprtkConverter::writeInt32},
                {"flt32", 2, false, false, &ExprtkConverter::writeFloat32},
                {"flt32bs", 2, true, false, &ExprtkConverter::writeFloat32},
                {"uint64", 4, false, true, &ExprtkConverter::writeUInt64},
                {"uint64bs", 4, true, true, &ExprtkConverter::writeUInt64},
                {"int64", 4, false, true, &ExprtkConverter::writeInt64},
                {"int64bs", 4, true, true, &ExprtkConverter::writeInt64},
                {"flt64", 4, false, false, &ExprtkConverter::writeFloat64},
                {"flt64bs", 4, true, false, &ExprtkConverter::writeFloat64},
            };
            return helpers;
        }

        /** Whether this platform can carry what the helper writes. */
        static bool isWriteHelperAvailable(const WriteHelper& pHelper) {
            return !pHelper.mNeedsExactInt64 || exprconv::sExactInt64;
        }

        /** The named helper, or writeHelpers().end() when this platform has no such name. */
        static WriteHelperIterator findWriteHelper(const std::string& pName) {
            const std::vector<WriteHelper>& helpers = writeHelpers();
            for (WriteHelperIterator it = helpers.begin(); it != helpers.end(); it++) {
                if (pName == it->mName && isWriteHelperAvailable(*it)) {
                    return it;
                }
            }
            return helpers.end();
        }

        /** The names worth suggesting here, so a gated one is never offered. */
        static std::string availableWriteHelpers() {
            std::vector<std::string> names;
            for (const WriteHelper& helper: writeHelpers()) {
                if (isWriteHelperAvailable(helper)) {
                    names.push_back(helper.mName);
                }
            }
            return strvecToString(names, ",");
        }

        int getWriteRegistersCount(int resultsCount) const {
            return mWriteHelper->mRegisterCount * resultsCount;
        }

        void writeRegisterValues(ModbusRegisters& pRegisters, long double pExprValue) const {
            try {
                (this->*mWriteHelper->mWriter)(pRegisters, pExprValue, mWriteHelper->mSwapBytes);
            } catch (const std::exception& ex) {
                throw ConvException(mWriteAs + " conversion failed: " + ex.what());
            } catch (...) {
                throw ConvException("Unknown error when converting "s + std::to_string(pExprValue) + " using " + mWriteAs);
            }
        }

        static void appendRegisters(ModbusRegisters& pRegisters, const std::vector<uint16_t>& pData) {
            for (size_t i = 0; i < pData.size(); i++) {
                pRegisters.appendValue(pData[i]);
            }
        }

        /**
         * Every writer narrows through MqttValue rather than casting. A floating
         * point to integer conversion that does not fit is undefined, so a cast
         * would corrupt the value before the range check meant to catch it ever
         * ran; MqttValue checks first and throws ConvException, which the caller
         * above already wraps.
         */
        void writeInt16(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const int32_t tmp = MqttValue::fromLongDouble(pExprValue).getInt();
            if (tmp < INT16_MIN || tmp > INT16_MAX) {
                throw ConvException(std::string("Conversion failed, value " + std::to_string(tmp) + " out of range"));
            }
            int16_t toWrite = tmp;
            toWrite = ConverterTools::setByteOrder(toWrite, pSwapBytes);
            pRegisters.appendValue(toWrite);
        }

        void writeUInt16(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const int32_t tmp = MqttValue::fromLongDouble(pExprValue).getInt();
            if (tmp < 0 || tmp > UINT16_MAX) {
                throw ConvException(std::string("Conversion failed, value " + std::to_string(tmp) + " out of range"));
            }
            uint16_t toWrite = tmp;
            toWrite = ConverterTools::setByteOrder(toWrite, pSwapBytes);
            pRegisters.appendValue(toWrite);
        }

        void writeInt32(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const int32_t value = MqttValue::fromLongDouble(pExprValue).getInt();
            appendRegisters(pRegisters, ConverterTools::numberToRegisters<int32_t>(value, mWriteLowFirst, pSwapBytes, 2));
        }

        void writeUInt32(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const int64_t value = MqttValue::fromLongDouble(pExprValue).getInt64();
            if (value < 0 || value > UINT32_MAX) {
                throw ConvException(std::string("Conversion failed, value " + std::to_string(value) + " out of range"));
            }
            appendRegisters(pRegisters, ConverterTools::numberToRegisters<uint32_t>(static_cast<uint32_t>(value), mWriteLowFirst, pSwapBytes, 2));
        }

        void writeFloat32(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            appendRegisters(pRegisters, ConverterTools::floatingPointToRegisters<float>(static_cast<float>(pExprValue), mWriteLowFirst, pSwapBytes, 2));
        }

        void writeInt64(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const int64_t value = MqttValue::fromLongDouble(pExprValue).getInt64();
            appendRegisters(pRegisters, ConverterTools::numberToRegisters<int64_t>(value, mWriteLowFirst, pSwapBytes, 4));
        }

        void writeUInt64(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const uint64_t value = MqttValue::fromLongDouble(pExprValue).getUInt64();
            appendRegisters(pRegisters, ConverterTools::numberToRegisters<uint64_t>(value, mWriteLowFirst, pSwapBytes, 4));
        }

        void writeFloat64(ModbusRegisters& pRegisters, long double pExprValue, bool pSwapBytes) const {
            const double value = MqttValue::fromLongDouble(pExprValue).getDouble();
            appendRegisters(pRegisters, ConverterTools::floatingPointToRegisters<double>(value, mWriteLowFirst, pSwapBytes, 4));
        }
    };
