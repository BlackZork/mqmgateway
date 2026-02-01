#pragma once

#include <exprtk.hpp>
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/converter.hpp"

std::string
strvecToString(const std::vector<std::string>& elements, const char *const delimiter) {
    std::ostringstream os;
    auto b = begin(elements), e = end(elements);

    if (b != e) {
        std::copy(b, prev(e), std::ostream_iterator<std::string>(os, delimiter));
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

            double ret = mExpression.value();

            if (mPrecision == 0)
                return MqttValue::fromInt(ret);

            return MqttValue::fromDouble(ret, mPrecision);
        }

        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            ModbusRegisters ret;

            mValues[0] = value.getDouble();

            double exprval = mExpression.value();

            typedef exprtk::results_context<double> results_context_t;
            const auto& results = mExpression.results();

            if (std::isnan(exprval)) {
                if (results.count() > 100)
                    throw ConvException("Too many values returned, max=100");

                int registersNeeded = getWriteRegistersCount((int)results.count());

                if (registersNeeded != registerCount)
                    throw ConvException("Got " + std::to_string(results.count()) + " values, need " + std::to_string(registerCount));

                typedef typename results_context_t::type_store_t type_t;

                for(std::size_t i = 0; i < results.count(); i++) {
                    type_t ts = results[i];
                    switch (ts.type) {
                        case type_t::e_scalar: {
                            double val = *(double*)(ts.data);
                            writeRegisterValues(ret, val);
                        } break;
                        case type_t::e_vector:
                            throw ConvException("Invalid list returned on position " + std::to_string(i));
                        break;
                        case type_t::e_string:
                            throw ConvException("Invalid string value returned on position " + std::to_string(i));
                        break;
                        case type_t::e_unknown:
                            throw ConvException("Unknown value type returned on position " + std::to_string(i));
                        break;
                    }
                }
            } else {
                int registersNeeded = getWriteRegistersCount(1);
                if (registerCount != registersNeeded)
                    throw ConvException("Got a single value, need " + std::to_string(registerCount));

                writeRegisterValues(ret, exprval);
                //ret.appendValue(toUInt16(exprval));
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
                throw ConvException(std::string("Exprtk ") + mParser.error());
            }

            mPrecision = values[ConverterArg::sPrecisionArgName].as_int();
            // for write only
            std::vector<std::string> writeHelpers {
                "int16",
                "int32",
                "int32bs",
                "uint32",
                "uint32bs",
                "flt32",
                "flt32bs"
            };

            mWriteAs = values["write_as"].as_str();

            if (!mWriteAs.empty()) {
                auto it = std::find(writeHelpers.begin(), writeHelpers.end(), mWriteAs);
                if (it == writeHelpers.end()) {
                    std::string helpersStr = strvecToString(writeHelpers, ",");
                    throw ConvException("Unknown write helper "s + mWriteAs + ", valid helpers:" + helpersStr);
                }
            }

            mWriteLowFirst = values[ConverterArg::sLowFirstArgName].as_bool();
        }

        virtual ~ExprtkConverter() {
            mExpression.release();
        }
    private:
        exprtk::symbol_table<double> mSymbolTable;
        exprtk::parser<double> mParser;
        exprtk::expression<double> mExpression;
        mutable std::vector<double> mValues;
        int mPrecision;
        std::string mWriteAs;
        bool mWriteLowFirst;

        static double int32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, false);
        }

        static double int32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<int32_t>(highRegister, lowRegister, true);
        }

        static double uint32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, false);
        }

        static double uint32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<uint32_t>(highRegister, lowRegister, true);
        }

        static double flt32(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, false);
        }

        static double flt32bs(const double highRegister, const double lowRegister) {
            return ConverterTools::toNumber<float>(highRegister, lowRegister, true);
        }

        static double int16(const double regValue) {
            uint16_t tmp = uint16_t(regValue);
            return (int16_t)tmp;
        }

        //write helpers
        int getWriteRegistersCount(int resultsCount) const {
            if (mWriteAs.empty() || mWriteAs == "int16")
                return resultsCount;
            return 2*resultsCount;
        }

        void writeRegisterValues(ModbusRegisters& ret, double exprval) const {
            try {
                if (mWriteAs.empty())
                    ret.appendValue(ConverterTools::toUInt16(exprval));
                else if (mWriteAs == "int16") {
                    int32_t val = exprval;
                    if (val < INT16_MIN || val > INT16_MAX)
                        throw ConvException(std::string("Conversion failed, value " + std::to_string(val) + " out of range"));
                    ret.appendValue(val);
                } else if (mWriteAs == "flt32") {
                    writeFloat32(ret, exprval, false);
                } else if (mWriteAs == "flt32bs") {
                    writeFloat32(ret, exprval, true);
                } else if (mWriteAs == "int32") {
                    writeInt32(ret, exprval, false);
                } else if (mWriteAs == "int32bs") {
                    writeInt32(ret, exprval, true);
                } else if (mWriteAs == "uint32") {
                    writeUInt32(ret, exprval, false);
                } else if (mWriteAs == "uint32bs") {
                    writeUInt32(ret, exprval, true);
                }
            } catch (const std::exception& ex) {
                throw ConvException(mWriteAs + " conversion failed: " + ex.what());
            } catch (...) {
                throw ConvException("Unknown error when converting "s + std::to_string(exprval) + " using " + mWriteAs);
            }
        }

        void writeFloat32(ModbusRegisters& ret, double exprval, bool swapBytes) const {
            assert(sizeof(float) == sizeof(int32_t));
            union {
                int32_t out_value;
                float in_value;
            } CastData;

            CastData.in_value = exprval;

            std::vector<uint16_t> regdata(
                ConverterTools::int32ToRegisters(CastData.out_value, mWriteLowFirst, swapBytes, 2)
            );

            ret.appendValue(regdata[0]);
            ret.appendValue(regdata[1]);
        }

        void writeInt32(ModbusRegisters& ret, double exprval, bool swapBytes) const {
            int32_t val = exprval;

            std::vector<uint16_t> regdata(
                ConverterTools::int32ToRegisters(val, mWriteLowFirst, swapBytes, 2)
            );

            ret.appendValue(regdata[0]);
            ret.appendValue(regdata[1]);
        }

        void writeUInt32(ModbusRegisters& ret, double exprval, bool swapBytes) const {
            union {
                int32_t out_value;
                uint32_t in_value;
            } CastData;

            CastData.in_value = exprval;

            std::vector<uint16_t> regdata(
                ConverterTools::int32ToRegisters(CastData.out_value, mWriteLowFirst, swapBytes, 2)
            );

            ret.appendValue(regdata[0]);
            ret.appendValue(regdata[1]);
        }


    };
