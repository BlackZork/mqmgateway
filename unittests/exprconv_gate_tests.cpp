#include <catch2/catch_all.hpp>

#include "exprconv/int64support.hpp"

#include "libmodmqttconv/converterplugin.hpp"
#include "libmodmqttsrv/config.hpp"
#include "libmodmqttsrv/dll_import.hpp"

#include "plugin_utils.hpp"

/**
 * The gate has two halves. Refusal is the absent registration: where a long
 * double is too narrow the helpers are never added to the symbol table, so
 * exprtk cannot compile an expression naming one. The hint below only explains
 * that refusal, which is why it can be asked for a width this machine does not
 * have.
 */
TEST_CASE("The 64-bit helper hint should") {
    SECTION("name the helper and the width when the platform is too narrow") {
        const std::optional<std::string> hint = exprconv::narrowLongDoubleHint("uint64(R0, R1, R2, R3)", 53);

        REQUIRE(hint.has_value());
        REQUIRE(hint->find("uint64") != std::string::npos);
        REQUIRE(hint->find("53") != std::string::npos);
    }

    SECTION("say nothing when the platform is wide enough") {
        REQUIRE_FALSE(exprconv::narrowLongDoubleHint("uint64(R0, R1, R2, R3)", 64).has_value());
    }

    SECTION("say nothing about an expression that names no 64-bit helper") {
        REQUIRE_FALSE(exprconv::narrowLongDoubleHint("uint32(R0, R1) * 2", 53).has_value());
    }

    SECTION("say nothing about a name that merely contains a helper name") {
        REQUIRE_FALSE(exprconv::narrowLongDoubleHint("my_uint64_scale * R0", 53).has_value());
    }

    SECTION("recognise every gated helper") {
        for (const char* const helper: exprconv::sInt64Helpers) {
            REQUIRE(exprconv::narrowLongDoubleHint(std::string(helper) + "(R0, R1, R2, R3)", 53).has_value());
        }
    }

    SECTION("say nothing about the float helpers, which are exact at any width") {
        REQUIRE_FALSE(exprconv::narrowLongDoubleHint("flt64(R0, R1, R2, R3)", 53).has_value());
        REQUIRE_FALSE(exprconv::narrowLongDoubleHint("flt64bs(R0, R1, R2, R3)", 53).has_value());
    }

    SECTION("name the byte swapped helper rather than the one inside it") {
        const std::optional<std::string> hint = exprconv::narrowLongDoubleHint("uint64bs(R0, R1, R2, R3)", 53);

        REQUIRE(hint.has_value());
        REQUIRE(hint->find("uint64bs") != std::string::npos);
    }
}

#ifdef HAVE_EXPRTK

TEST_CASE("A configured 64-bit helper should") {
    PluginLoader loader("../exprconv/exprconv.so");

    std::shared_ptr<DataConverter> conv(loader.getConverter("evaluate"));
    ConverterArgValues args(conv->getArgs());
    args.setArgValue("expression", "uint64(R0, R1, R2, R3)");

#if LDBL_MANT_DIG >= 64
    SECTION("be accepted where a long double carries 64 bits exactly") {
        REQUIRE_NOTHROW(conv->setArgValues(args));
    }
#else
    // the branch arm/v6 and arm/v7 take in CI
    SECTION("be refused where a long double is only as wide as a double") {
        REQUIRE_THROWS_AS(conv->setArgValues(args), ConvException);
    }

    SECTION("be refused with the platform named") {
        try {
            conv->setArgValues(args);
            FAIL("expected a ConvException");
        } catch (const ConvException& ex) {
            REQUIRE(std::string(ex.what()).find("not available on this platform") != std::string::npos);
        }
    }
#endif
}

#endif
