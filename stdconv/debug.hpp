#pragma once

#include <cmath>
#include <cstdio>

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
                addInt16Section(doc, alloc, pData.getValue(0));
            } else {
                addInt32Section(doc, alloc, pData);
                addUInt32Section(doc, alloc, pData);
                addFloat32Section(doc, alloc, pData);
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

        // Keys are the converter call strings — copy-paste into config.yaml
        static void addInt16Section(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                    uint16_t pVal) {
            uint16_t swapped = ConverterTools::setByteOrder(pVal, true);
            rapidjson::Value intVals(rapidjson::kObjectType);
            intVals.AddMember("std.int16", (int16_t)pVal, pAlloc);
            intVals.AddMember("std.int16(swap_bytes=true)", (int16_t)swapped, pAlloc);
            pDoc.AddMember("int16", intVals, pAlloc);

            rapidjson::Value uintVals(rapidjson::kObjectType);
            uintVals.AddMember("std.uint16", pVal, pAlloc);
            uintVals.AddMember("std.uint16(swap_bytes=true)", swapped, pAlloc);
            pDoc.AddMember("uint16", uintVals, pAlloc);
        }

        static void addInt32Section(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                    const ModbusRegisters& pData) {
            rapidjson::Value intVals(rapidjson::kObjectType);
            intVals.AddMember("std.int32",
                              ConverterTools::registersToInt32(pData.values(), false, false), pAlloc);
            intVals.AddMember("std.int32(low_first=true)",
                              ConverterTools::registersToInt32(pData.values(), true, false), pAlloc);
            intVals.AddMember("std.int32(swap_bytes=true)",
                              ConverterTools::registersToInt32(pData.values(), false, true), pAlloc);
            intVals.AddMember("std.int32(low_first=true,swap_bytes=true)",
                              ConverterTools::registersToInt32(pData.values(), true, true), pAlloc);
            pDoc.AddMember("int32", intVals, pAlloc);
        }

        static void addUInt32Section(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                     const ModbusRegisters& pData) {
            rapidjson::Value uintVals(rapidjson::kObjectType);
            uintVals.AddMember("std.uint32",
                               (uint32_t)ConverterTools::registersToInt32(pData.values(), false, false), pAlloc);
            uintVals.AddMember("std.uint32(low_first=true)",
                               (uint32_t)ConverterTools::registersToInt32(pData.values(), true, false), pAlloc);
            uintVals.AddMember("std.uint32(swap_bytes=true)",
                               (uint32_t)ConverterTools::registersToInt32(pData.values(), false, true), pAlloc);
            uintVals.AddMember("std.uint32(low_first=true,swap_bytes=true)",
                               (uint32_t)ConverterTools::registersToInt32(pData.values(), true, true), pAlloc);
            pDoc.AddMember("uint32", uintVals, pAlloc);
        }

        static void addFloat32Section(rapidjson::Document& pDoc, rapidjson::Document::AllocatorType& pAlloc,
                                      const ModbusRegisters& pData) {
            rapidjson::Value floatVals(rapidjson::kObjectType);
            addFloat(floatVals, pAlloc, "std.float32",
                     ConverterTools::toNumber<float>(pData.getValue(0), pData.getValue(1), false));
            addFloat(floatVals, pAlloc, "std.float32(low_first=true)",
                     ConverterTools::toNumber<float>(pData.getValue(1), pData.getValue(0), false));
            addFloat(floatVals, pAlloc, "std.float32(swap_bytes=true)",
                     ConverterTools::toNumber<float>(pData.getValue(0), pData.getValue(1), true));
            addFloat(floatVals, pAlloc, "std.float32(low_first=true,swap_bytes=true)",
                     ConverterTools::toNumber<float>(pData.getValue(1), pData.getValue(0), true));
            pDoc.AddMember("float32", floatVals, pAlloc);
        }

        static void addFloat(rapidjson::Value& pVals, rapidjson::Document::AllocatorType& pAlloc,
                             const char* pKey, float pVal) {
            if (std::isnan(pVal))
                pVals.AddMember(rapidjson::Value(pKey, pAlloc), rapidjson::Value("nan", pAlloc), pAlloc);
            else if (std::isinf(pVal))
                pVals.AddMember(rapidjson::Value(pKey, pAlloc),
                                rapidjson::Value(pVal > 0.0f ? "inf" : "-inf", pAlloc), pAlloc);
            else
                pVals.AddMember(rapidjson::Value(pKey, pAlloc),
                                rapidjson::Value(static_cast<double>(pVal)), pAlloc);
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
