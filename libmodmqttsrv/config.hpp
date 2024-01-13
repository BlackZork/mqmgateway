#pragma once
#include <cstdint>
#include <chrono>
#include <regex>

#include <yaml-cpp/yaml.h>
#include <boost/version.hpp>

#include "libmodmqttsrv/exceptions.hpp"

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

        Type mType;
        std::string mName = "";
        std::chrono::milliseconds mResponseTimeout = std::chrono::seconds(1);
        std::chrono::milliseconds mResponseDataTimeout = std::chrono::seconds(0);
        std::chrono::milliseconds mMinDelayBeforePoll = std::chrono::seconds(0);

        //RTU only
        std::string mDevice = "";
        int mBaud = 0;
        char mParity = '\0';
        int mDataBit = 0;
        int mStopBit = 0;
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
