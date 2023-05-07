#pragma once
#include <cstdint>
#include <yaml-cpp/yaml.h>
#include "exceptions.hpp"
#include <boost/version.hpp>

namespace modmqttd {

#if BOOST_VERSION >= 107600 // 1.76.0
#define boost_dll_import boost::dll::import_symbol
#else
#define boost_dll_import boost::dll::import
#endif

class ConfigurationException : public ModMqttException {
    public:
        ConfigurationException(const YAML::Mark& mark, const char* what);
        ConfigurationException(const YAML::Mark& mark, const std::string& what) : ConfigurationException(mark, what.c_str()) {};
};

class ConfigTools {
    public:
        template <typename T>
        static T readRequiredValue(const YAML::Node& node) {
            if (!node.IsScalar()) {
                std::string err = "string expected, list/null found";
                throw ConfigurationException(node.Mark(), err);
            }
            return node.as<T>();
        }

        template <typename T>
        static T readRequiredValue(const YAML::Node& parent, const char* nodeName) {

            const YAML::Node& node = parent[nodeName];
            if (!node.IsDefined()) {
                std::string err = std::string("Missing reqired property '") + nodeName + "'";
                throw ConfigurationException(parent.Mark(), err);
            }
            return readRequiredValue<T>(node);
        }

        static std::string readRequiredString(const YAML::Node& parent, const char* nodeName) {
            std::string ret(readRequiredValue<std::string>(parent, nodeName));
            if (ret.empty()) {
                std::string err = std::string(nodeName) + " is an empty string";
                throw ConfigurationException(parent.Mark(), err);
            }
            return ret;
        }

        template <typename T>
        static bool readOptionalValue(T& pDest, const YAML::Node& parent, const char* nodeName) {
            const YAML::Node& node = parent[nodeName];
            if (!node.IsDefined())
                return false;

            if (!node.IsScalar()) {
                std::string err = std::string(nodeName) + " must have a single value. List/null found";
                throw ConfigurationException(parent.Mark(), err);
            }

            pDest = node.as<T>();
            return true;
        }
};


class ModbusNetworkConfig {
    public:
        typedef enum {
            RTU,
            TCPIP
        } Type;

        typedef enum {
            NONE,
            UP,
            DOWN
        } RtuRtsMode;

        typedef enum {
            UNSPECIFIED,
            RS232,
            RS485
        } RtuSerialMode;

        ModbusNetworkConfig() {}
        ModbusNetworkConfig(const YAML::Node& source);
        bool isSameAs(const ModbusNetworkConfig& other) {
            if (mName != other.mName || mType != other.mType)
                throw ModMqttProgramException("Cannot compare config change for diffrent modbus networks");
            switch(mType) {
                case RTU:
                    return mDevice == other.mDevice &&
                            mBaud == other.mBaud &&
                            mParity == other.mParity &&
                            mDataBit == other.mDataBit &&
                            mStopBit == other.mStopBit &&
                            mRtuSerialMode == other.mRtuSerialMode &&
                            mRtsMode == other.mRtsMode &&
                            mRtsDelayUs == other.mRtsDelayUs;
                break;
                case TCPIP:
                    return mAddress == other.mAddress && mPort == other.mPort;
                break;
                default:
                    throw ModMqttProgramException("Unknown config type");
            }
        };

        //RTU only
        Type mType;
        std::string mName = "";
        std::string mDevice = "";
        int mBaud = 0;
        char mParity = '\0';
        uint8_t mDataBit = 0;
        uint8_t mStopBit = 0;
        RtuSerialMode mRtuSerialMode = RtuSerialMode::UNSPECIFIED;
        RtuRtsMode mRtsMode = RtuRtsMode::NONE;
        int mRtsDelayUs = 0;
        
        //TCP only
        std::string mAddress = "";
        int mPort = 0;
};

class MqttBrokerConfig {
    public:
        MqttBrokerConfig() {};
        MqttBrokerConfig(const YAML::Node& source);
        bool isSameAs(const MqttBrokerConfig& other) {
            return mHost == other.mHost &&
                    mPort == other.mPort &&
                    mKeepalive == other.mKeepalive &&
                    mUsername == other.mUsername &&
                    mPassword == other.mPassword;
        }

        //defaults are from mosqittopp.h
        std::string mHost;
        int mPort = 1883;
        int mKeepalive = 60;
        std::string mUsername;
        std::string mPassword;

        std::string mClientId;
};

}
