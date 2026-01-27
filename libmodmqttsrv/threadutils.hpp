#pragma once

#include "defs.h"
#include "logging.hpp"
#include <sstream>

namespace modmqttd {

class ThreadUtils {
    public:
        static void set_thread_name(const char* threadName) {
        #ifdef HAVE_PTHREAD_SETNAME_NP
            pthread_setname_np(pthread_self(),threadName);
            g_thread_name = threadName;
        #else
            /*
                Log thread id for each named thread for
                easy identification in logfile.
            */
            std::ostringstream tname;
            tname << std::this_thread::get_id();
            spdlog::info("Thread {} id is {}", threadName, tname.str());
        #endif
    }
};

}
