#include <catch2/catch_all.hpp>

#include <string>
#include <utility>
#include <vector>

#include "libmodmqttconv/converterplugin.hpp"

#include "plugin_utils.hpp"

TEST_CASE("every std converter should declare the register count it is designed for") {
    PluginLoader loader("../stdconv/stdconv.so");

    // 0 means the converter works with any number of registers: divide and
    // multiply read one or two, the rest are driven by the configured count.
    const std::vector<std::pair<std::string, int>> expected({{"bit", 1},
                                                             {"bitmask", 1},
                                                             {"int8", 1},
                                                             {"uint8", 1},
                                                             {"int16", 1},
                                                             {"uint16", 1},
                                                             {"int32", 2},
                                                             {"uint32", 2},
                                                             {"float32", 2},
                                                             {"divide", 0},
                                                             {"multiply", 0},
                                                             {"scale", 0},
                                                             {"string", 0},
                                                             {"map", 0},
                                                             {"debug", 0}});

    for (std::vector<std::pair<std::string, int>>::const_iterator it = expected.begin(); it != expected.end(); it++) {
        INFO("converter std." << it->first);
        std::shared_ptr<DataConverter> conv(loader.getConverter(it->first));
        REQUIRE(conv != nullptr);
        REQUIRE(conv->getExpectedRegisterCount() == it->second);
    }
}
