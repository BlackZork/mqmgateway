#include "mqttpayload.hpp"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


namespace modmqttd {

void
createConvertedValue(
    rapidjson::Writer<rapidjson::StringBuffer>& writer,
    const MqttValue& value
) {
    switch(value.getSourceType()) {
        case MqttValue::SourceType::INT:
            writer.Int(value.getInt());
            break;
        case MqttValue::SourceType::DOUBLE: {
            int prec = value.getDoublePrecision();
            if (prec != MqttValue::NO_PRECISION)
                writer.SetMaxDecimalPlaces(prec);
            writer.Double(value.getDouble());
            break;
        }
        case MqttValue::SourceType::BINARY:
            writer.String(static_cast<const char*>(value.getBinaryPtr()), value.getBinarySize());
            break;
        case MqttValue::SourceType::INT64:
            writer.Int64(value.getInt64());
            break;
    }
}


bool isMap(const std::vector<MqttObjectDataNode>& pNodes) {
    // map with one or more elements
    return !pNodes.front().isUnnamed();
}


bool isList(const std::vector<MqttObjectDataNode>& pNodes) {
    // cannot create a single element list for now
    return pNodes.size() > 1 && pNodes.front().isUnnamed();
}


void
generateJson(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const std::vector<MqttObjectDataNode>& pNodes) {
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
    const std::vector<MqttObjectDataNode>& nodes(pObj.mState.getNodes());


    if (nodes.size() == 1) {
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
