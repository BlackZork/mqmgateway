#define CATCH_CONFIG_CONSOLE_WIDTH 300
#define CATCH_CONFIG_RUNNER
#include "catch2/catch_all.hpp"

#include "libmodmqttsrv/logging.hpp"
#include "libmodmqttsrv/threadutils.hpp"

#include "timing.hpp"

int main( int argc, char* argv[] ) {
  modmqttd::ThreadUtils::set_thread_name("utest");

  modmqttd::Log::severity loglevel = modmqttd::Log::severity::trace;
  if (const char* env_p = std::getenv("MQM_TEST_LOGLEVEL")) {
      loglevel = static_cast<modmqttd::Log::severity>(std::atoi(env_p) - 1);
  }
  modmqttd::Log::init_logging(loglevel);

  timing::init();

  int result = Catch::Session().run( argc, argv );

  // global clean-u u goes here

  return result;
}
