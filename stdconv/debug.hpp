#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <type_traits>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "libmodmqttconv/converter.hpp"
#include "libmodmqttconv/convexception.hpp"
#include "libmodmqttconv/convtools.hpp"

class DebugConverter : public DataConverter {
    public:
        virtual MqttValue toMqtt(const ModbusRegisters& pData) const {
            rapidjson::Document doc;
            doc.SetObject();
            auto& alloc = doc.GetAllocator();

            addRawArrays(doc, alloc, pData);

            if (pData.getCount() == 1) {
                addIntegerSection<int16_t>(doc, alloc, pData, "int16", sByteOrders);
                addIntegerSection<uint16_t>(doc, alloc, pData, "uint16", sByteOrders);
            } else if (pData.getCount() == 2) {
                addIntegerSection<int32_t>(doc, alloc, pData, "int32", sRegisterOrders);
                addIntegerSection<uint32_t>(doc, alloc, pData, "uint32", sRegisterOrders);
                addFloatingPointSection<float>(doc, alloc, pData, "float32", sRegisterOrders);
            } else if (pData.getCount() == 4) {
                addIntegerSection<int64_t>(doc, alloc, pData, "int64", sRegisterOrders);
                addIntegerSection<uint64_t>(doc, alloc, pData, "uint64", sRegisterOrders);
                addFloatingPointSection<double>(doc, alloc, pData, "float64", sRegisterOrders);
            }

            addStringSection(doc, alloc, pData);

            rapidjson::StringBuffer sb;
            if (mPrettyPrint) {
                rapidjson::PrettyWriter<rapidjson::StringBuffer> w(sb);
                doc.Accept(w);
            } else {
                rapidjson::Writer<rapidjson::StringBuffer> w(sb);
                doc.Accept(w);
            }

            return MqttValue::fromBinary(sb.GetString(), sb.GetSize());
        }

        virtual ModbusRegisters toModbus(const MqttValue&, int) const {
            throw ConvException("std.debug is read-only");
        }

        virtual ConverterArgs getArgs() const {
            ConverterArgs ret;
            ret.add("pretty_print", ConverterArgType::BOOL, false);
            return ret;
        }

        virtual void setArgValues(const ConverterArgValues& pArgs) {
            mPrettyPrint = pArgs["pretty_print"].as_bool();
        }

    private:
        bool mPrettyPrint = false;

        static void addRawArrays(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                 const ModbusRegisters& pData) {
            rapidjson::Value rawArr(rapidjson::kArrayType);
            rapidjson::Value hexArr(rapidjson::kArrayType);
            for (int i = 0; i < pData.getCount(); i++) {
                rawArr.PushBack(pData.getValue(i), pAlloc);
                char buf[8];
                std::snprintf(buf, sizeof(buf), "0x%04X", pData.getValue(i));
                hexArr.PushBack(rapidjson::Value(buf, pAlloc), pAlloc);
            }
            pDoc.AddMember("raw", rawArr, pAlloc);
            pDoc.AddMember("hex", hexArr, pAlloc);
        }

        /**
         * The word and byte orders a converter can be asked for, in the order
         * they are reported. A single register has no word order to vary.
         */
        struct RegisterOrder {
                bool mLowFirst;
                bool mSwapBytes;
        };
        static constexpr RegisterOrder sRegisterOrders[] = {{false, false}, {true, false}, {false, true}, {true, true}};
        static constexpr RegisterOrder sByteOrders[] = {{false, false}, {false, true}};

        /** The converter call string that produces this reading, to paste into config.yaml. */
        static std::string converterKey(const char* pName, const RegisterOrder& pOrder) {
            std::string ret("std.");
            ret += pName;
            if (pOrder.mLowFirst && pOrder.mSwapBytes) {
                ret += "(low_first=true,swap_bytes=true)";
            } else if (pOrder.mLowFirst) {
                ret += "(low_first=true)";
            } else if (pOrder.mSwapBytes) {
                ret += "(swap_bytes=true)";
            }
            return ret;
        }

        static rapidjson::Value jsonKey(const std::string& pKey, rapidjson::Document::AllocatorType& pAlloc) {
            return rapidjson::Value(pKey.c_str(), static_cast<rapidjson::SizeType>(pKey.size()), pAlloc);
        }

        template <typename T, size_t N>
        static void addIntegerSection(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                      const ModbusRegisters& pData, const char* pName, const RegisterOrder (&pOrders)[N]) {
            rapidjson::Value vals(rapidjson::kObjectType);
            for (size_t i = 0; i < N; i++) {
                const T val = ConverterTools::registersToNumber<T>(pData.values(), pOrders[i].mLowFirst, pOrders[i].mSwapBytes);
                if constexpr (std::is_signed<T>::value) {
                    vals.AddMember(jsonKey(converterKey(pName, pOrders[i]), pAlloc), rapidjson::Value(static_cast<int64_t>(val)), pAlloc);
                } else {
                    vals.AddMember(jsonKey(converterKey(pName, pOrders[i]), pAlloc), rapidjson::Value(static_cast<uint64_t>(val)), pAlloc);
                }
            }
            pDoc.AddMember(jsonKey(pName, pAlloc), vals, pAlloc);
        }

        template <typename T, size_t N>
        static void addFloatingPointSection(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                            const ModbusRegisters& pData, const char* pName, const RegisterOrder (&pOrders)[N]) {
            rapidjson::Value vals(rapidjson::kObjectType);
            for (size_t i = 0; i < N; i++) {
                const T val = ConverterTools::registersToFloatingPoint<T>(pData.values(), pOrders[i].mLowFirst, pOrders[i].mSwapBytes);
                addFloatingPointValue(vals, pAlloc, converterKey(pName, pOrders[i]), val);
            }
            pDoc.AddMember(jsonKey(pName, pAlloc), vals, pAlloc);
        }

        /** json has no nan or infinity, so those readings are reported as strings. */
        static void addFloatingPointValue(rapidjson::Value& pVals, rapidjson::Document::AllocatorType& pAlloc,
                                          const std::string& pKey, double pVal) {
            if (std::isnan(pVal)) {
                pVals.AddMember(jsonKey(pKey, pAlloc), rapidjson::Value("nan", pAlloc), pAlloc);
            } else if (std::isinf(pVal)) {
                pVals.AddMember(jsonKey(pKey, pAlloc), rapidjson::Value(pVal > 0.0 ? "inf" : "-inf", pAlloc), pAlloc);
            } else {
                pVals.AddMember(jsonKey(pKey, pAlloc), rapidjson::Value(pVal), pAlloc);
            }
        }

        static void addStringSection(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                     const ModbusRegisters& pData) {
            std::string str;
            for (int i = 0; i < pData.getCount(); i++) {
                uint16_t val = pData.getValue(i);
                str += static_cast<char>((val >> 8) & 0xFF);
                str += static_cast<char>(val & 0xFF);
            }
            pDoc.AddMember("string", rapidjson::Value(str.c_str(), str.size(), pAlloc), pAlloc);
        }
};
