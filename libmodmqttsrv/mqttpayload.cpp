#include "mqttpayload.hpp"

#include <cmath>
#include <cstring>
#include <optional>
#include <stdexcept>

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>


namespace modmqttd {

/**
 * The spelling of NaN or an infinity, or nullptr for any other value. NaN has
 * one spelling whatever its sign bit, which iostream would print as -nan.
 */
static const char*
nonFiniteToken(const MqttValue& pValue) {
    switch (pValue.getSourceType()) {
    case MqttValue::SourceType::DOUBLE:
    case MqttValue::SourceType::FLOAT64: {
        const long double value = pValue.getLongDouble();
        if (std::isnan(value)) {
            return "NaN";
        }
        if (std::isinf(value)) {
            return value > 0 ? "Infinity" : "-Infinity";
        }
        return nullptr;
    }
    case MqttValue::SourceType::BINARY:
    case MqttValue::SourceType::INT:
    case MqttValue::SourceType::INT64:
    case MqttValue::SourceType::UINT64:
        return nullptr;
    }
    throw std::logic_error("Unhandled MqttValue source type " + std::to_string(pValue.getSourceType()));
}

static void
writeIntegral(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const MqttValue::IntegralValue& pIntegral) {
    if (pIntegral.isSigned()) {
        pWriter.Int64(pIntegral.asInt64());
    } else {
        pWriter.Uint64(pIntegral.asUInt64());
    }
}

std::string
MqttPayload::encodeScalar(const MqttValue& pValue, InfNanMode pMode) {
    const char* token = nonFiniteToken(pValue);
    if (token == nullptr) {
        return pValue.getString();
    }
    switch (pMode) {
    case InfNanMode::JSON_NULL:
        return "null";
    case InfNanMode::LITERAL:
        return token;
    case InfNanMode::STRING:
        return std::string("\"") + token + "\"";
    }
    throw std::logic_error("Unhandled InfNanMode");
}

void
createConvertedValue(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const MqttValue& pValue, InfNanMode pMode) {
    // Before any precision handling: Writer::Double() refuses NaN and the
    // infinities, writing nothing at all, and narrowing them to an integer
    // throws.
    const char* token = nonFiniteToken(pValue);
    if (token != nullptr) {
        switch (pMode) {
        case InfNanMode::JSON_NULL:
            pWriter.Null();
            return;
        case InfNanMode::LITERAL:
            pWriter.RawValue(token, std::strlen(token), rapidjson::kNumberType);
            return;
        case InfNanMode::STRING:
            pWriter.String(token);
            return;
        }
        throw std::logic_error("Unhandled InfNanMode");
    }

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
            // a whole value goes through asIntegral() as in the FLOAT64 arm, so
            // one above INT64_MAX is still written; a fractional one loses its
            // decimals, as asked
            const std::optional<MqttValue::IntegralValue> integral = pValue.asIntegral();
            if (integral.has_value()) {
                writeIntegral(pWriter, *integral);
            } else {
                pWriter.Int64(pValue.getInt64());
            }
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
        //
        // A precision of zero or less asks for no decimal places at all, so it
        // takes the integer path too. rapidjson's dtoa() asserts
        // maxDecimalPlaces >= 1, and writer.Double() would in any case round a
        // 64 bit result through a double after the caller asked to keep it.
        int prec = pValue.getDoublePrecision();
        if (prec <= 0) {
            const std::optional<MqttValue::IntegralValue> integral = pValue.asIntegral();
            if (integral.has_value()) {
                writeIntegral(pWriter, *integral);
                return;
            }
            if (prec != MqttValue::NO_PRECISION) {
                // a fractional value whose decimals were asked away, the same
                // way the DOUBLE arm drops them
                pWriter.Int64(pValue.getInt64());
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
generateJson(rapidjson::Writer<rapidjson::StringBuffer>& pWriter, const MqttObjectDataNodeList& pNodes, InfNanMode pMode) {
    if (isMap(pNodes)) {
        pWriter.StartObject();
        for(const MqttObjectDataNode& node: pNodes) {
            pWriter.Key(node.getName().c_str());
            if (node.isScalar() || node.hasConverter()) {
                MqttValue v = node.getConvertedValue();
                createConvertedValue(pWriter, v, pMode);
            } else {
                generateJson(pWriter, node.getChildNodes(), pMode);
            }
        }
        pWriter.EndObject();
    } else if (isList(pNodes)) {
        pWriter.StartArray();
        for(const MqttObjectDataNode& node: pNodes) {
            if (node.isScalar() || node.hasConverter()) {
                MqttValue v = node.getConvertedValue();
                createConvertedValue(pWriter, v, pMode);
            } else {
                generateJson(pWriter, node.getChildNodes(), pMode);
            }
        }
        pWriter.EndArray();
    } else {
        //single scalar
        MqttValue v = pNodes.front().getConvertedValue();
        createConvertedValue(pWriter, v, pMode);
    }
}


std::string
MqttPayload::generate(const MqttObject& pObj) {
    const MqttObjectDataNodeList& nodes(pObj.mState.getNodes());


    if (!nodes.outputAsList()) {
        const MqttObjectDataNode& single(nodes[0]);
        if (single.isUnnamed() && (single.isScalar() || single.hasConverter())) {
            MqttValue v = single.getConvertedValue();
            return encodeScalar(v, pObj.getInfNanMode());
        }
    };
    // single non-scalar node or a list
    {
        rapidjson::StringBuffer ret;
        rapidjson::Writer<rapidjson::StringBuffer> writer(ret);
        generateJson(writer, nodes, pObj.getInfNanMode());
        return ret.GetString();
    }
}

}
