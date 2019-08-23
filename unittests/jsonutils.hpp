#pragma once

#include "catch2/catch.hpp"
#include <string>
#include "rapidjson/document.h"

void REQUIRE_JSON(const std::string& current, const char* expected);
