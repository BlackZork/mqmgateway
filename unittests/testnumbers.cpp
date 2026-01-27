#include "testnumbers.hpp"

namespace TestNumbers {

const float Float::ABCD_as_float = *(const float*)&Float::ABCD;
const float Float::CDAB_as_float = *(const float*)&Float::CDAB;
const float Float::DCBA_as_float = *(const float*)&Float::DCBA;
const float Float::BADC_as_float = *(const float*)&Float::BADC;

} //namespace