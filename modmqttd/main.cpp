#include <iostream>

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include "libmodmqttsrv/common.hpp"
#include "libmodmqttsrv/modmqtt.hpp"
#include "config.hpp"

namespace args = boost::program_options;
using namespace std;

modmqttd::ModMqtt server;

void logCriticalError(std::shared_ptr<boost::log::sources::severity_logger<modmqttd::Log::severity>>& log,
        const char* message)
{
    if (log != NULL)
        BOOST_LOG_SEV(*log, modmqttd::Log::critical) << message;
    else
        cerr << message << endl;
}

int main(int ac, char* av[]) {
    std::shared_ptr<boost::log::sources::severity_logger<modmqttd::Log::severity>> log;
    std::string configPath;
    try {
        args::options_description desc("Arguments");

        int logLevel;

        desc.add_options()
            ("help", "produce help message")
            ("loglevel, l", args::value<int>(&logLevel), "setup logging: 0 off, 1-6 sets loglevel, higher is more verbose")
            ("config, c", args::value<string>(&configPath), "path to configuration file")
        ;

        args::variables_map vm;
        args::store(args::parse_command_line(ac, av, desc), vm);
        args::notify(vm);

        if (vm.count("help")) {
            cout << desc << "\n";
            return EXIT_SUCCESS;
        }

        modmqttd::Log::severity level = modmqttd::Log::severity::info;
        if (vm.count("loglevel")) {
            level = (modmqttd::Log::severity)(vm["loglevel"].as<int>());
        }

        modmqttd::Log::init_logging(level);
        // temporary logger used in main before classes are initalized
        log.reset(new boost::log::sources::severity_logger<modmqttd::Log::severity>());
        // TODO add version information
        BOOST_LOG_SEV(*log, modmqttd::Log::info) << "modmqttd is starting";

        server.init(configPath);
        server.start();

        BOOST_LOG_SEV(*log, modmqttd::Log::info) << "modmqttd stopped";
        return EXIT_SUCCESS;
    } catch (const YAML::BadFile& ex) {
        if (configPath == "")
            configPath = boost::filesystem::current_path().native();
        std::string msg = "Failed to load configuration from "s + configPath;
        logCriticalError(log, msg.c_str());
    } catch (const std::exception& ex) {
        logCriticalError(log, ex.what());
    } catch (...) {
        logCriticalError(log, "Unknown initialization error occured");
    }
    return EXIT_FAILURE;
}
