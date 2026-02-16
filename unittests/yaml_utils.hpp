#pragma once

#include <yaml-cpp/yaml.h>
#include <set>

//https://stackoverflow.com/questions/70426203/iterate-through-all-nodes-in-yaml-cpp-including-recursive-anchor-alias
class YAMLScalarVisitor {
    public:
        using Callback = std::function<void(const std::string&, YAML::Node&)>;
        YAMLScalarVisitor(Callback cb): mSeen(), cb(cb) {}

        void operator()(YAML::Node &cur) {
            mSeen.push_back(cur);
            if (cur.IsMap()) {
                for (YAML::iterator pair = cur.begin(); pair != cur.end(); ++pair) {
                    mMapKey = pair->first.as<std::string>();
                    descend(pair->second);
                }
            } else if (cur.IsSequence()) {
                mMapKey.clear();
                for (YAML::iterator it = cur.begin(); it != cur.end(); ++it)
                    descend(it->first);
            } else if (cur.IsScalar()) {
                if (!mMapKey.empty())
                    cb(mMapKey, cur);
                mMapKey.clear();
            }
        }
    private:
        void descend(YAML::Node &target) {
            if (std::find(mSeen.begin(), mSeen.end(), target) != mSeen.end())
                (*this)(target);
        }

        std::vector<YAML::Node> mSeen;
        std::string mMapKey;
        Callback cb;
};


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

            YAML::Emitter out;
            out << mYAML;

            mEmitted = true;

            adjustTimings();

            return out.c_str();
        }

    YAML::Node mYAML;
    std::set<std::string> mTimeNodes;

    private:
        bool mEmitted = false;

        void adjustTimings() {
            const std::set<std::string>& tn = mTimeNodes;
            YAMLScalarVisitor([tn](const std::string& key, YAML::Node &scalar) {
                if (tn.find(key) != tn.end()) {
                    //TODO parse and scale
                    //scalar = YAML::Node("test");
                }
            })(mYAML);
        }
};
