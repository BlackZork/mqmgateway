#define CATCH_CONFIG_CONSOLE_WIDTH 300
#define CATCH_CONFIG_RUNNER
#include "catch2/catch_all.hpp"
#include "libmodmqttsrv/logging.hpp"

int main( int argc, char* argv[] ) {

  modmqttd::Log::severity loglevel = modmqttd::Log::severity::debug;
  if (const char* env_p = std::getenv("MQM_TEST_LOGLEVEL")) {
      loglevel = static_cast<modmqttd::Log::severity>(std::atoi(env_p) - 1);
  }
  modmqttd::Log::init_logging(loglevel);

  int result = Catch::Session().run( argc, argv );

  // global clean-up…

  return result;
}
