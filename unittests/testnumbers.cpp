#include "testnumbers.hpp"

#include <cstring>

namespace TestNumbers {

namespace {

/**
 * Reinterprets a register bit pattern as the value of type T it encodes.
 */
template <typename T, typename TBits>
T
fromBits(TBits pBits) {
    static_assert(sizeof(T) == sizeof(TBits), "bit pattern must be as wide as the value");
    T ret;
    std::memcpy(&ret, &pBits, sizeof(ret));
    return ret;
}

} // namespace

const float Float::ABCD_as_float = fromBits<float>(Float::ABCD);
const float Float::CDAB_as_float = fromBits<float>(Float::CDAB);
const float Float::DCBA_as_float = fromBits<float>(Float::DCBA);
const float Float::BADC_as_float = fromBits<float>(Float::BADC);

const double Double::ABCDEFGH_as_double = fromBits<double>(Double::ABCDEFGH);
const double Double::GHEFCDAB_as_double = fromBits<double>(Double::GHEFCDAB);
const double Double::BADCFEHG_as_double = fromBits<double>(Double::BADCFEHG);
const double Double::HGFEDCBA_as_double = fromBits<double>(Double::HGFEDCBA);

} // namespace
