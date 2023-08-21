#include "jsonutils.hpp"

using namespace rapidjson;

void REQUIRE_JSON(const std::string& current, const char* expected) {
    Document d_current, d_expected;

    d_current.Parse(current.c_str());
    d_expected.Parse(expected);

    CAPTURE(current.c_str());
    CAPTURE(expected);
    REQUIRE(d_current == d_expected);
}
