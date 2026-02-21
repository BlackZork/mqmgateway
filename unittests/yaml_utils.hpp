#pragma once

#include <yaml-cpp/yaml.h>
#include <set>

#include "libmodmqttsrv/yaml_converters.hpp"

#include "timing.hpp"

class TestConfig {
    public:
        TestConfig(const std::string& strdata) {
            mYAML = YAML::Load(strdata);

            mTimeNodes.insert("refresh");
            mTimeNodes.insert("refresh");
            mTimeNodes.insert("watch_period");
            mTimeNodes.insert("delay_before_command");
            mTimeNodes.insert("delay_before_first_command");
        }

        std::string toString() {
            // avoid fatal bugs when
            // a single config object is used by two
            // separate test cases and each one modify it
            // in this case tests can fail depending on Catch2 test run order
            if (mEmitted)
                throw new std::logic_error("Cannot emit config twice - use SECTIONs and a config object in TEST_CASE");

            adjustTimings();

            YAML::Emitter out;
            out << mYAML;

            mEmitted = true;

            std::string ret(out.c_str());
            spdlog::trace(ret);
            return ret;
        }

    YAML::Node mYAML;
    std::set<std::string> mTimeNodes;

    private:
        bool mEmitted = false;

        void adjustTimings() {
            if (timing::getFactor() == 1)
                return;
            traverse(mYAML, "", mYAML);
        }

        void traverse(YAML::Node& parent, const std::string& key, YAML::Node& node) {
            if (node.IsMap()) {
                for (YAML::iterator it = node.begin(); it != node.end(); ++it) {
                    traverse(node, it->first.as<std::string>(), it->second);
                }
            }
            else if (node.IsSequence()) {
                for (YAML::iterator it = node.begin(); it != node.end(); ++it) {
                    YAML::Node item(*it);
                    traverse(node, "", item);
                }
            }
            else if (node.IsScalar()) {
                const std::set<std::string>& tn = mTimeNodes;
                if (tn.find(key) != tn.end()) {
                    const std::regex re("([0-9]+)(ms|s|min)");
                    std::cmatch matches;
                    std::string strval = node.as<std::string>();

                    if (!std::regex_match(strval.c_str(), matches, re))
                        FAIL(strval + " cannot be adjusted");

                    int mval = std::stoi(matches[1]);
                    std::string unit = matches[2];

                    int aval = mval * timing::getFactor();

                    parent[key] = std::to_string(aval) + unit;
                }
            }
        }
};
