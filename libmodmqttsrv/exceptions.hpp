#pragma once

#include <exception>
#include <string>

namespace modmqttd {

class ModMqttException : public std::exception {
    public:
        ModMqttException() { mWhat = "Unknown error"; }
        ModMqttException(const char* what) : mWhat(what) {}
        ModMqttException(const std::string& what) : mWhat(what) {}
        virtual const char* what() const throw() { return mWhat.c_str(); }
    protected:
        std::string mWhat;
};

class ModMqttProgramException : public ModMqttException{
    public:
        ModMqttProgramException(const std::string& what) : ModMqttException(what) {}
};

class MosquittoException : public ModMqttException {
    public:
        MosquittoException(const std::string& what) : ModMqttException(what) {}
};

class ObjectCommandNotFoundException : public ModMqttException {
    public:
        ObjectCommandNotFoundException(const char* topic) {
            mWhat = std::string("Command for topic ") + topic + " not found";
        }
};

class MqttPayloadConversionException : public ModMqttException {
    public:
        MqttPayloadConversionException(const std::string& what) : ModMqttException(what) {}
};

class ConvNameParserException : public ModMqttException {
    public:
        ConvNameParserException(const std::string& what) : ModMqttException(what) {}
};

class ConvPluginNotFoundException : public ModMqttException {
    public:
        ConvPluginNotFoundException(const std::string& what) : ModMqttException(what) {}
};


}
