#pragma once

#include "logging.hpp"
#include "defs.h"

namespace modmqttd {

class ThreadUtils {
    public:
        static void set_thread_name(const char* threadName) {
        #ifdef HAVE_PTHREAD_SETNAME_NP
            pthread_setname_np(pthread_self(),threadName);
        #else
        #endif
    }
};

}
