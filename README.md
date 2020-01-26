# MQMGateway - MQTT gateway for modbus networks
A multithreaded C++ service that exposes data from multiple [Modbus](http://www.modbus.org/) networks as [MQTT](http://mqtt.org/) topics.

Main features:
* Connects to multiple TCP and RTU modbus networks
* Handles state and availablity for each configured MQTT object
* Flexible MQTT state topic configuration:
  * single modbus register published as string value
  * multiple modbus registers as JSON object
  * multiple modubs registers as JSON list
  * registers from different slaves combined as single JSON list/object
* Data conversion:
  * single register value to MQTT converters
  * multiple registers values to single MQTT value converters
  * support for custom conversion plugins
* Fast modbus frequency polling, configurable per newtork, per mqtt object and per register
* Optimized modbus pulling - registers used in multiple MQTT topics are polled only once

MQMGateway depends on [libmodbus](https://libmodbus.org/) and [Mosqutto](https://mosquitto.org/) MQTT library. See main [CMakeLists.txt](link) for full list of dependencies. It is developed under Linux, but it should be easy to port it to other platforms.

# License
This software is dual-licensed:
  * under the terms of [AGPL-3.0 license](https://www.gnu.org/licenses/agpl-3.0.html) as Open Source project.
  * under commercial license.

For a commercial-friendly license and support please see http://mqmgateway.zork.pl. 

# Third-party licenses

This software includes "A single-producer, single-consumer lock-free queue for C++" written by 
Cameron Desrochers. See license terms in [LICENSE.md](readerwriterqueue/LICENSE.md)

# Installation

1. `git clone https://github.com/BlackZork/mqmgateway.git`
1. Install dependencies:
   1. boost
   1. libmodbus
   1. mosquitto
   1. yaml-cpp
   1. rapidJSON
1. Configure project:

    ```
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -S (project dir) -B (build dir)
    make 
    make install
    ```

1. Copy config.template.yml to /etc/modmqttd.yml.

# Configuration

modmqttd configuration file is in YAML format. It is divied in three main sections:

* modmqttd section contains information about custom plugins
* modbus section contains modbus network definitions
* mqtt section contains mqtt broker connection parameters and modbus register mappings

For quick example see [config.template.yaml](modmqttd/config.template.yaml) in source directory.

## Configuration values

* timespan format: "([0-9]+)(ms|s|min)"

  Used for various timeout configuration entries

## modmqttd section

* converter_search_path (optional)

  List of paths where to search for converter plugins. If path does not start with '/' then it is treated as relative to current working directory

* converter_plugins (optional)

  List of converter plugins to load. Modmqttd search for plugins in all directories specified in converter_search_path list

## modbus section

Modbus section contains a list of modbus networks modmqttd should connect to.
Modbus network configuration parameters are listed below:

* name (required)

  Unique name for network - referenced in mqtt mappings

* response_timeout (optional, default )

  A timespan 

There is an example modmqttd.service file for systemd. 

