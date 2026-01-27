#include <yaml-cpp/yaml.h>

class TestConfig {
    public:
        TestConfig(const std::string& strdata) {
            mYAML = YAML::Load(strdata);
        }

        std::string toString() {
            // avoid fatal bugs when
            // a single config object is used by two
            // separate test cases and each one modify it
            // in this case tests can fail depending on Catch2 test run order
            if (mEmitted)
                throw new std::logic_error("Cannot emit config twice - use SECTIONs and a config object in TEST_CASE");

                YAML::Emitter out;
            out << mYAML;

            mEmitted = true;

            return out.c_str();
        }

    YAML::Node mYAML;
    bool mEmitted = false;
};
