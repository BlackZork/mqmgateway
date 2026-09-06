#pragma once

#include <array>
#include <cctype>
#include <limits>
#include <optional>
#include <string>

/**
 * What the exprtk converter needs to know about the width of a long double,
 * kept apart from expr.hpp so that it can be tested without instantiating the
 * exprtk engine.
 */
namespace exprconv {

/** Mantissa bits a long double needs before it holds every uint64 exactly. */
constexpr int sExactMantissaBits = 64;

/** What this platform actually has: 64 on amd64 and i386, 113 on arm64, 53 on 32-bit arm. */
constexpr int sMantissaBits = std::numeric_limits<long double>::digits;

/**
 * Whether the 64-bit integer expression helpers can be offered here at all.
 * Where this is false they are never registered, so exprtk refuses to compile
 * an expression naming one and the daemon does not start.
 */
constexpr bool sExactInt64 = sMantissaBits >= sExactMantissaBits;

/**
 * The helpers gated on that, which are the integer ones alone. An integer read
 * from registers is often a bitmask, where a single drifted bit is a wrong
 * answer rather than an imprecise one, so a mantissa too narrow to hold it must
 * refuse rather than round.
 *
 * flt64 and flt64bs are deliberately absent. They read an IEEE-754 double out
 * of the registers, and a long double is at least as wide as a double
 * everywhere, so that value arrives exact on every platform - including the
 * 32-bit arm targets where a long double is a double. Gating them would refuse
 * a configuration that works.
 */
constexpr std::array<const char*, 4> sInt64Helpers = {"int64", "int64bs", "uint64", "uint64bs"};

/** Whether pName appears in pExpression as a whole word rather than inside a longer one. */
inline bool
namesHelper(const std::string& pExpression, const std::string& pName) {
    const auto identChar = [](char pChar) { return std::isalnum(static_cast<unsigned char>(pChar)) != 0 || pChar == '_'; };

    for (size_t pos = pExpression.find(pName); pos != std::string::npos; pos = pExpression.find(pName, pos + 1)) {
        const size_t end = pos + pName.size();
        const bool leftFree = pos == 0 || !identChar(pExpression[pos - 1]);
        const bool rightFree = end >= pExpression.size() || !identChar(pExpression[end]);
        if (leftFree && rightFree) {
            return true;
        }
    }
    return false;
}

/**
 * Explains why an expression could not name a 64-bit helper, or nothing when it
 * names none or when pMantissaBits is wide enough to carry them.
 *
 * The width is a parameter rather than read from the platform so that the
 * narrow answer can be asked for, and asserted on, from any machine.
 *
 * Only ever consulted once a parse has already failed, so matching a name that
 * was never the cause - inside a string literal, say - costs nothing. That is
 * what keeps this out of the decision itself: the registration is what refuses
 * a helper, this only says why.
 */
inline std::optional<std::string>
narrowLongDoubleHint(const std::string& pExpression, int pMantissaBits = sMantissaBits) {
    if (pMantissaBits >= sExactMantissaBits) {
        return std::nullopt;
    }

    for (const char* const helper: sInt64Helpers) {
        if (namesHelper(pExpression, helper)) {
            return std::string(helper) + " is not available on this platform: its long double has " + std::to_string(pMantissaBits) + " bits of mantissa, too few to hold a 64-bit value exactly. Use the std.int64, std.uint64 or std.float64 converter instead";
        }
    }
    return std::nullopt;
}

} // namespace exprconv
