#include <iostream>
#include <filesystem>

#include "argh/argh.h"
#include <memory>
#include <pthread.h>
#include "libmodmqttsrv/common.hpp"
#include "libmodmqttsrv/modmqtt.hpp"
#include "config.hpp"
#include "libmodmqttsrv/logging.hpp"
#include "libmodmqttsrv/threadutils.hpp"
#include "version.hpp"

using namespace std::string_literals;

modmqttd::ModMqtt server;

void logCriticalError(const char* message) {
    if (spdlog::get(modmqttd::Log::loggerName) != NULL)
        spdlog::critical(message);
    else
        std::cerr << message << std::endl;
}

constexpr const char* USAGE = R"(
Usage:
  -c, --config     path to configuration file
  -l, --loglevel   setup logging: 0 off, 1-6 sets loglevel, higher is more verbose
  -v, --version    print modmqttd version
  --help           this help message
)";

int main(int ac, char* av[]) {
    modmqttd::ThreadUtils::set_thread_name("main");
    std::string configPath;

    try {
        argh::parser cmdl(ac, av, argh::parser::PREFER_PARAM_FOR_UNREG_OPTION);
        cmdl.add_params({ "-l", "--loglevel", "-c", "--config" });

        int logLevel = -1;

        for(auto& flag: cmdl.flags()) {
            if (flag == "help") {
                std::cout << "modmqttd v." << FULL_VERSION << std::endl;
                std::cout << USAGE << std::endl;
                return EXIT_SUCCESS;
            } else if (flag == "version" || flag == "v") {
                std::cout << FULL_VERSION << std::endl;
                return EXIT_SUCCESS;
            } else {
                std::cerr << "Unknown flag '" << flag << "', use --help for available options" << std::endl;
                return EXIT_FAILURE;
            }
        }

        for(auto param: cmdl.params()) {
            if (param.first == "config" || param.first == "c") {
                configPath = param.second;
            } else if (param.first == "loglevel" || param.first == "l") {
                if (!(cmdl({"l", "loglevel"}) >> logLevel)) {
                    std::cerr << "Cannot parse loglevel value" << std::endl;
                    return EXIT_FAILURE;
                }
                if (logLevel < 0 || logLevel > 6) {
                    std::cerr << "loglevel must be between 0 and 6" << std::endl;
                    return EXIT_FAILURE;
                }
            }
        }

        server.init(logLevel, configPath);
        server.start();

        spdlog::info("modmqttd stopped");
        return EXIT_SUCCESS;
    } catch (const YAML::BadFile& ex) {
        if (configPath == "")
            configPath = std::filesystem::current_path().native();
        std::string msg = "Failed to load configuration from "s + configPath;
        logCriticalError(msg.c_str());
    } catch (const std::exception& ex) {
        logCriticalError(ex.what());
    } catch (...) {
        logCriticalError("Unknown initialization error occured");
    }
    return EXIT_FAILURE;
}
