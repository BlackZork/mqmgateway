#pragma once
#include <cstdint>
#include <chrono>
#include <regex>

#include <yaml-cpp/yaml.h>

#include "exceptions.hpp"
#include "logging.hpp"

namespace modmqttd {

class ConfigurationException : public ModMqttException {
    public:
        ConfigurationException(const YAML::Mark& mark, const char* what);
        ConfigurationException(const YAML::Mark& mark, const std::string& what) : ConfigurationException(mark, what.c_str()) {};
        int mLineNumber = 0;
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
        static YAML::Node setOptionalValueFromNode(T& pDest, const YAML::Node& parent, const char* nodeName) {
            const YAML::Node& node = parent[nodeName];
            if (node.IsDefined()) {

                if (!node.IsScalar()) {
                    std::string err = std::string(nodeName) + " must have a single value. List/null found";
                    throw ConfigurationException(parent.Mark(), err);
                }
                pDest = node.as<T>();
            }

            return node;
        }

        template <typename T>
        static bool readOptionalValue(T& pDest, const YAML::Node& parent, const char* nodeName) {
            return setOptionalValueFromNode(pDest, parent, nodeName).IsDefined();
        }
};

class ModbusWatchdogConfig {
    public:
        std::chrono::milliseconds mWatchPeriod = std::chrono::seconds(10);
        std::string mDevicePath;
};

class ModbusNetworkConfig {
    static constexpr std::chrono::milliseconds MAX_RESPONSE_TIMEOUT = std::chrono::milliseconds(999);

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
        std::chrono::milliseconds mResponseTimeout = std::chrono::milliseconds(500);
        std::chrono::milliseconds mResponseDataTimeout = std::chrono::seconds(0);

        bool hasDelayBeforeCommand() const { return mDelayBeforeCommand != nullptr; }
        bool hasDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand != nullptr; }

        const std::shared_ptr<std::chrono::milliseconds>& getDelayBeforeCommand() const { return mDelayBeforeCommand; }
        const std::shared_ptr<std::chrono::milliseconds>& getDelayBeforeFirstCommand() const { return mDelayBeforeFirstCommand; }

        void setDelayBeforeCommand(const std::chrono::milliseconds& pDelay) { mDelayBeforeCommand.reset(new std::chrono::milliseconds(pDelay)); }
        void setDelayBeforeFirstCommand(const std::chrono::milliseconds& pDelay) { mDelayBeforeFirstCommand.reset(new std::chrono::milliseconds(pDelay)); }

        unsigned short mMaxWriteRetryCount = 2;
        unsigned short mMaxReadRetryCount = 1;


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

        ModbusWatchdogConfig mWatchdogConfig;
    private:
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeCommand;
        std::shared_ptr<std::chrono::milliseconds> mDelayBeforeFirstCommand;
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
                    mPassword == other.mPassword &&
                    mTLS == other.mTLS &&
                    mCafile == other.mCafile;
        }

        //defaults are from mosquittopp.h
        std::string mHost;
        int mPort = 1883;
        int mKeepalive = 60;
        std::string mUsername;
        std::string mPassword;

        std::string mClientId;

        bool mTLS = false;
        std::string mCafile;
};

}
