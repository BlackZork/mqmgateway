#pragma once

#include "defs.h"
#include "exceptions.hpp"
#include "logging.hpp"

// https://stackoverflow.com/questions/58177815/how-to-actually-detect-musl-libc
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
    #undef _GNU_SOURCE /* don't contaminate other includes unnecessarily */
#else
    #include <features.h>
    #ifndef __USE_GNU
        #define __MUSL__
    #endif
#endif

namespace modmqttd {

typedef enum {
    ON_CHANGE=1,
    EVERY_POLL=2,
    ONCE=3
} PublishMode;

/**
 * How a NaN or infinite value is written to a payload: as JSON null, as the
 * bare NaN / Infinity / -Infinity tokens (not JSON, but what some parsers
 * read), or as those spellings in a JSON string.
 */
enum class InfNanMode {
    JSON_NULL,
    LITERAL,
    STRING
};
}
