#include "mqttpayload.hpp"

#include <optional>
#include <stdexcept>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


namespace modmqttd {

void
createConvertedValue(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const MqttValue& pValue) {
    switch (pValue.getSourceType()) {
    case MqttValue::SourceType::INT:
        pWriter.Int(pValue.getInt());
        return;
    case MqttValue::SourceType::DOUBLE: {
        int prec = pValue.getDoublePrecision();
        if (prec > 0) {
            pWriter.SetMaxDecimalPlaces(prec);
        } else if (prec == MqttValue::NO_PRECISION) {
            // same as sstream default, see MqttValue::format(T pValue)
            pWriter.SetMaxDecimalPlaces(6);
        }

        if (prec == 0) {
            pWriter.Int64(pValue.getInt64());
        } else {
            pWriter.Double(pValue.getDouble());
        }
        return;
    }
    case MqttValue::SourceType::BINARY:
        pWriter.String(static_cast<const char*>(pValue.getBinaryPtr()), pValue.getBinarySize());
        return;
    case MqttValue::SourceType::INT64:
        pWriter.Int64(pValue.getInt64());
        return;
    case MqttValue::SourceType::UINT64:
        pWriter.Uint64(pValue.getUInt64());
        return;
    case MqttValue::SourceType::FLOAT64: {
        // A whole value is written as an integer so that a 64 bit result keeps
        // every digit; writer.Double() would round it to a double first. Same
        // rule MqttValue::format() applies to the scalar payload, shared via
        // asIntegral() so the two cannot disagree.
        int prec = pValue.getDoublePrecision();
        if (prec == MqttValue::NO_PRECISION) {
            const std::optional<MqttValue::IntegralValue> integral = pValue.asIntegral();
            if (integral.has_value()) {
                if (integral->isSigned()) {
                    pWriter.Int64(integral->asInt64());
                } else {
                    pWriter.Uint64(integral->asUInt64());
                }
                return;
            }
        }
        pWriter.SetMaxDecimalPlaces(prec == MqttValue::NO_PRECISION ? 6 : prec);
        pWriter.Double(pValue.getDouble());
        return;
    }
    }
    // Each arm returns, so reaching here means a source type has no arm. Not a
    // ConvException: that is a bug, not bad device data, and must not be
    // swallowed by the publish path error handling. A default: label would
    // silence -Wswitch, the only compile time check for a missing arm.
    throw std::logic_error("Unhandled MqttValue source type " + std::to_string(pValue.getSourceType()));
}


bool isMap(const MqttObjectDataNodeList& pNodes) {
    // map with one or more elements
    return !pNodes.front().isUnnamed();
}


bool isList(const MqttObjectDataNodeList& pNodes) {
    // cannot create a single element list for now
    return pNodes.outputAsList() || (pNodes.size() > 1 && pNodes.front().isUnnamed());
}


void
generateJson(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const MqttObjectDataNodeList& pNodes) {
    if (isMap(pNodes)) {
        pWriter.StartObject();
        for(const MqttObjectDataNode& node: pNodes) {
            pWriter.Key(node.getName().c_str());
            if (node.isScalar() || node.hasConverter()) {
                MqttValue v = node.getConvertedValue();
                createConvertedValue(pWriter, v);
            } else {
                generateJson(pWriter, node.getChildNodes());
            }
        }
        pWriter.EndObject();
    } else if (isList(pNodes)) {
        pWriter.StartArray();
        for(const MqttObjectDataNode& node: pNodes) {
            if (node.isScalar() || node.hasConverter()) {
                MqttValue v = node.getConvertedValue();
                createConvertedValue(pWriter, v);
            } else {
                generateJson(pWriter, node.getChildNodes());
            }
        }
        pWriter.EndArray();
    } else {
        //single scalar
        MqttValue v = pNodes.front().getConvertedValue();
        createConvertedValue(pWriter, v);
    }
}


std::string
MqttPayload::generate(const MqttObject& pObj) {
    const MqttObjectDataNodeList& nodes(pObj.mState.getNodes());


    if (!nodes.outputAsList()) {
        const MqttObjectDataNode& single(nodes[0]);
        if (single.isUnnamed() && (single.isScalar() || single.hasConverter())) {
            MqttValue v = single.getConvertedValue();
            return v.getString();
        }
    };
    // single non-scalar node or a list
    {
        rapidjson::StringBuffer ret;
        rapidjson::Writer<rapidjson::StringBuffer> writer(ret);
        generateJson(writer, nodes);
        return ret.GetString();
    }
}

}
