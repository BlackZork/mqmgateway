#include "catch2/catch_all.hpp"
#include "config.hpp"
#include "yaml_utils.hpp"

using namespace modmqttd;
using namespace Catch::Matchers;

TEST_CASE("MQTT configuration") {
TestConfig config(R"(
mqtt:
  client_id: mqtt_test
  broker:
    host: localhost
  objects:
    - topic: test_sensor
      state:
        register: tcptest.1.2
)");

    SECTION("for broker") {
        YAML::Node broker = config.mYAML["mqtt"]["broker"];

        SECTION("without TLS node") {
            MqttBrokerConfig cut(broker);

            SECTION("should have TLS disabled and default port") {
                REQUIRE_FALSE(cut.mTLS);
                REQUIRE(cut.mPort == 1883);
            }

            SECTION("should be compared consistently to config with TLS") {
                MqttBrokerConfig cut(broker);
                MqttBrokerConfig clone(broker);
                broker["tls"] = YAML::Node(YAML::NodeType::Map);
                MqttBrokerConfig other(broker);

                REQUIRE(cut.isSameAs(clone));
                REQUIRE(clone.isSameAs(cut));
                REQUIRE_FALSE(cut.isSameAs(other));
                REQUIRE_FALSE(other.isSameAs(cut));
            }
        }

        SECTION("with TLS node") {
            broker["tls"] = YAML::Node(YAML::NodeType::Map);
            YAML::Node tls = broker["tls"];
            const int customPort = 2183;
            const std::string cafile = "/dev/null";

            SECTION("should have TLS enabled") {
                MqttBrokerConfig cut(broker);

                REQUIRE(cut.mTLS);
            }

            SECTION("without port node should have default TLS port") {
                MqttBrokerConfig cut(broker);

                REQUIRE(cut.mPort == 8883);
            }

            SECTION("and port node should have custom port") {
                broker["port"] = customPort;
                MqttBrokerConfig cut(broker);

                REQUIRE(cut.mPort == customPort);
            }

            SECTION("and without cafile should be compared consistently to config with cafile") {
                MqttBrokerConfig cut(broker);
                MqttBrokerConfig clone(broker);
                tls["cafile"] = cafile;
                MqttBrokerConfig other(broker);

                REQUIRE(cut.isSameAs(clone));
                REQUIRE(clone.isSameAs(cut));
                CAPTURE(cut.mCafile);
                CAPTURE(other.mCafile);
                REQUIRE_FALSE(cut.isSameAs(other));
                REQUIRE_FALSE(other.isSameAs(cut));
            }

            SECTION("and cafile") {
                tls["cafile"] = cafile;

                SECTION("should have cafile") {
                    MqttBrokerConfig cut(broker);

                    REQUIRE(cut.mCafile == cafile);
                }

                SECTION("should be compared consistently") {
                    MqttBrokerConfig cut(broker);
                    MqttBrokerConfig clone(broker);

                    REQUIRE(cut.isSameAs(clone));
                    REQUIRE(clone.isSameAs(cut));
                }

                SECTION("should throw an exception when file is missing") {
                    const std::string missingFile = "/non-existant";
                    tls["cafile"] = missingFile;

                    REQUIRE_THROWS_MATCHES(MqttBrokerConfig(broker), ConfigurationException, MessageMatches(ContainsSubstring(missingFile)));
                }

                SECTION("should throw an exception when file is a directory") {
                    const std::string invalidFile = "/tmp";
                    tls["cafile"] = invalidFile;

                    REQUIRE_THROWS_MATCHES(MqttBrokerConfig(broker), ConfigurationException, MessageMatches(ContainsSubstring(invalidFile)));
                }
            }
        }
    }
}
