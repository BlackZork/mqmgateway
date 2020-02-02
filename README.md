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

  Used for various timeout interval configuration entries

## modmqttd section

* **converter_search_path** (optional)

  List of paths where to search for converter plugins. If path does not start with '/' then it is treated as relative to current working directory

* **converter_plugins** (optional)

  List of converter plugins to load. Modmqttd search for plugins in all directories specified in converter_search_path list

## modbus section

Modbus section contains a list of modbus networks modmqttd should connect to.
Modbus network configuration parameters are listed below:

* **name** (required)

  Unique name for network - referenced in mqtt mappings

* **response_timeout** (optional, default ?)

  A timeout interval used to wait for modbus response. This setting is propagated down mqtt object and register definitions. See modbus_set_response_timeout(3)

* **response_data_timeout** (optional, default ?)

  A timeout interval used to wait for data when reading response from modbus device. See modbus_set_byte_timeout(3).

* RTU device settings
  For details, see modbus_new_rtu(3)

  * **device** (required)

    A path to modbus RTU device

  * **baud** (required)

    The baud rate of the communication

  * **parity** (required)

    N for none, E for even, O for odd

  * **data_bit** (required)

    the number of bits of data (5,6,7,8)

  * **stop_bit** (required)

    the bits of stop

* TCP/IP device settings

  * **address**

    IP address of a device

  * **port**

    TCP port of a device

## MQTT section

The mqtt section contains broker definition and modbus register mappings. Mappings describe how modbus data should be published as mqtt topics.

* **client_id** (required)

  name used to connect to mqtt broker. 

* **refresh** (timespan, optional, default 5s)

  A timespan used to poll modbus registers. This setting is propagated
  down to object and register definitions

* **broker** (required)

  This section contains configuration settings used to connect to MQTT broker.

  * **host** (required)

    MQTT boker IP address

  * **port** (optional, default 1883)

    MQTT broker TCP port 

  * **keepalive** (optional, default 60s)

    The number of seconds after which the bridge should send a ping if no other traffic has occurred.

  * **username** (optional)

    The username to be used to connect to MQTT broker

  * **password** (optional)

    The password to be used to connect to MQTT broker

* **objects** (required)

A list of topics where modbus values are published to MQTT broker and subscribed for writing data received from MQTT broker to modbus registers.  

* **topic** (required)

  The base name for modbus register mapping. A mapping must contain at least one of following sections: 

    - *commands* - for writing to modbus registers
    - *state* - for reading modbus registers
    - *avaiability* - for checking if modbus data is available.

### Topic default values:

  * **response_timeout** (optional)

  Overrides modbus.response_timeout for this topic

  * **response_data_timeout** (optional)

    Overrides modbus.response_data_timeout for this topic

  * **refresh**

    Overrides mqtt.refresh for all state and availability sections in this topic

  A *commands* section.

  A single command is defined using following settings.

  * **name** (required)

    A command topic name to subscribe. Full name is created as `topic.name/command.name`

  * **register** (required)

    Modbus register address in the form of <network_name>.<slave_id>.<address>

  *  **register_type** (required)

    Modbus register type: coil, input, holding

  M2MGateway expects mqtt data as UTF-8 string value. It is converted to u_int16 and written to modbus register.

### The *state* section

  The state sections defines how to publish modbus data to MQTT broker.
  State can be mapped to a single register, an unnamed and a named list of registers. Following table shows what kind of output is generated for each type:

  | Value type | Default output |
  | --- | --- |
  | single register | u_int16 register data as string |
  | unnamed list | JSON array with u_int16 register data as string |
  | named list | JSON map with values as u_int16 register data as string |

  It is also possible to combine and output an unnamed list of registers as a single value using converter. See converters section for details.

  * **refresh**

    Overrides mqtt.refresh for this state topic

  * **response_timeout** (optional)

    Overrides modbus.response_timeout when reading state registers

  * **response_data_timeout** (optional)

    Overrides modbus.response_data_timeout when reading state registers

  When state is a single modbus register value:

  * **name** 

    The last part of topic name where value should be published. Full topic name is created as `topic.name/state.name`

  * **register** (required)

    Modbus register address in the form of <network_name>.<slave_id>.<address>

  * **register_type** (required)

    Modbus register type: coil, input, holding

  * **converter** (optional)

    The name of function that should be called to convert register u_int16 value to MQTT UTF-8 value. Format of function name is `library name.function name`. See converters for details. 

  The following examples show how to combine *name*, *register*, *register_type*, and *converter* to output different state values:

  1. single value 

    ```
    state:
      name: mqtt_val
      register: net.1.12
      register_type: coil
    ```

  2. unnamed list

    ```
    state:
      name: mqtt_list
      registers:
        - register: net.1.12
          register_type: input
        - register: net.1.13
          register_type: input
    ```

  3. multiple registers converted to single MQTT value

    ```
    state:
      name: mqtt_combined_val
      converter: std.int32
      registers:
        - register: net.1.12
          register_type: input
        - register: net.1.13
          register_type: input
    ```

  4. named list (map)

    ```
    state:
      registers:
        - name: humidity
          register: net.1.12
          register_type: input
          # optional
          converter: std.divide(100,2)
        - name:  temp1
          register: net.1.13
          register_type: input
    ```

  In all of above examples *refresh*, *response_timeout* and *response_data_timeout* can be added at any level to set different values to 
  whole list or a single register.

### The *availability* section

For every *state* topic there is another *availability* topic defined by default. If all data from modbus registers needed for *state* is read without errors then by default value "1" is published. If there is any network or device error when polling register data value "0" is published. This is the default behavoiur if *availability* section is not defined.

Availablity flag is always published before state value. 

*Availability* section extends this default behaviour by defining a single or list of modbus registers that should be readed to check if state data is valid. This could be i.e. some fault indicator or hardware switch state.

Configuration values:
 
  * **name** (required)

    The last part of topic name where availability flag should be published. Full topic name is created as `topic.name/availability.name`

  * **register** (required)

    Modbus register address in the form of <network_name>.<slave_id>.<address>

  * **register_type** (required)

    Modbus register type: coil, input, holding

  * **available_value** (optional, default 1)

    Expected u_int16 value when state is available. If other value is readed then availability flag is set to "0".

*register*, *register_type* and *available_value* can form a list when multiple registers should be readed.

## Data conversion



There is an example modmqttd.service file for systemd. 

