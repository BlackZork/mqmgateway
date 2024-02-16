#include <yaml-cpp/yaml.h>

class TestConfig {
    public:
        TestConfig(const std::string& strdata) {
            mYAML = YAML::Load(strdata);
        }

        std::string toString() const {
            YAML::Emitter out;
            out << mYAML;
            return out.c_str();
        }

    YAML::Node mYAML;
};
