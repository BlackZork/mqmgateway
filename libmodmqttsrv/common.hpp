#pragma once

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
