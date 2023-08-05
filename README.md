# MQMGateway - MQTT gateway for modbus networks
A multithreaded C++ service that exposes data from multiple [Modbus](http://www.modbus.org/) networks as [MQTT](http://mqtt.org/) topics.

Main features:
* Connects to multiple TCP and RTU modbus networks
* Handles state and availability for each configured MQTT object
* Allows to read and write to MODBUS registers from MQTT side with custom data conversion
* Flexible MQTT state topic configuration:
  * single modbus register published as string value
  * multiple modbus registers as JSON object
  * multiple modubs registers as JSON list
  * registers from different slaves combined as single JSON list/object
* Full control over modbus data polling
  * read multiple register values once for multiple topics
  * read multiple register values for a single topic one by one
  * read multiple register values for a single topic once
  * registers used in multiple MQTT topics are polled only once
* Data conversion:
  * single register value to MQTT converters
  * multiple registers values to single MQTT value converters
  * support for [exprtk](https://github.com/ArashPartow/exprtk) expressions language when converting data
  * support for custom conversion plugins
  * support for conversion in both directions
* Fast modbus frequency polling, configurable per newtork, per mqtt object and per register

MQMGateway depends on [libmodbus](https://libmodbus.org/) and [Mosquitto](https://mosquitto.org/) MQTT library. See main [CMakeLists.txt](CMakeLists.txt) for full list of dependencies. It is developed under Linux, but it should be easy to port it to other platforms.

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
   1. Catch2 (optional, for unit tests)

1. Configure and build project:

    ```
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -S (project dir) -B (build dir)
    make
    make install
    ```

    You can add -DWITHOUT_TESTS=1 to skip build of unit test executable.

1. Copy config.template.yaml to /etc/modmqttd/config.yaml.

1. Edit configuration and start service:

```
  systemctl start modmqttd
```


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

    the number of data bits (5,6,7,8)

  * **stop_bit** (required)

    serial port stop bit value (0 or 1)

  * **rtu_serial_mode** (optional)

    serial port mode: rs232 or rs485. See modbus_rtu_set_serial_mode(3)

  * **rtu_rts_mode** (optional)

    modbus Request To Send mode (up or down). See modbus_rtu_set_rts(3)

  * **rtu_rts_delay_us** (optional)

    modbus Request To Send delay period in microseconds. See modbus_rtu_set_rts_delay(3)

* TCP/IP device settings

  * **address**

    IP address of a device

  * **port**

    TCP port of a device

* **poll_groups**

    An optional list of modbus register address ranges that will be polled with a single modbus_read_registers(3) call.

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

  * **network**

    Sets default network name for all state, commands and availability sections in this topic

  * **slave**

    Sets default modbus slave address for all state, commands and availability sections in this topic

### A *commands* section.

  A single command is defined using following settings.

  * **name** (required)

    A command topic name to subscribe. Full name is created as `topic_name/command_name`

  * **register** (required)

    Modbus register address in the form of `<network_name>.<slave_id>.<register_number>`
    If `register_number` is a decimal, then first register address is 1.
    If `register_number` is a hexadecimal, then first register address is 0.

    `network_name` and `slave_id` are optional if default values are set for a topic

  *  **register_type** (required)

     Modbus register type: coil, holding

  *  **count** (optional)

     Number of registers to write. If set to > 1, then modbus_write_registers(3)/modbus_write_bits(3) is called.

  * **converter** (optional)

    The name of function that should be called to convert mqtt value to uint16_t value. Format of function name is `plugin name.function name`. See converters for details.

  Example of MQTT command topic declaration:

     objects:
    - topic: test_switch
      commands:
        - name: set
          register: tcptest.1.2
          register_type: holding
          converter: std.divide(100)

  Publishing value 100 to topic test_switch/set will write value 1 to register 2 on slave 1.

  Unless you provide a custom converter MQMGateway expects register value as UTF-8 string value or json array with register values. You must provide exactly the same number of values as registers to write.

### The *state* section

  The state sections defines how to publish modbus data to MQTT broker.
  State can be mapped to a single register, an unnamed and a named list of registers. Following table shows what kind of output is generated for each type:

  | Value type | Default output |
  | --- | --- |
  | single register | uint16_t register data as string |
  | unnamed list | JSON array with uint16_t register data as string |
  | named list | JSON map with values as uint16_t register data as string |

  It is also possible to combine and output an unnamed list of registers as a single value using converter. See converters section for details.

  Register list can be defined in two ways:

  1. As starting register and count:

    state:
      name: mqtt_combined_val
      converter: std.int32
      register: net.1.12
      count: 2

  This declaration creates a poll group. Poll group is read from modbus slave using a single
  modbus_read_registers(3) call. Overlapping poll groups are merged with each other and with
  poll groups defined in modbus section.

  2. as list of registers:

    state:
      - name: humidity
        register: net.1.12
        register_type: input
        # optional
        converter: std.divide(100,2)
      - name:  temp1
        register: net.1.300
        register_type: input

  This declaration do not create a poll group, but allows to construct MQTT topic data
  from different slaves, even on different modbus networks. On exception is if there are poll groups defined in modbus section, that overlaps state register definitions. In this case
  data is polled using poll group.

  * **refresh**

    Overrides mqtt.refresh for this state topic

  * **response_timeout** (optional)

    Overrides modbus.response_timeout when reading state registers

  * **response_data_timeout** (optional)

    Overrides modbus.response_data_timeout when reading state registers

  When state is a single modbus register value:

  * **name**

    The last part of topic name where value should be published. Full topic name is created as `topic_name/state_name`

  * **register** (required)

    Modbus register address in the form of <network_name>.<slave_id>.<register_number>
    If `register_number` is a decimal, then first register address is 1.
    If `register_number` is a hexadecimal, then first register address is 0.

    `network_name` and `slave_id` are optional if default values are set for a topic

  * **register_type** (optional, default: `holding`)

    Modbus register type: coil, bit, input, holding

  * **count**

     If defined, then this describes register range to poll. Register range is always
     polled with a single modbus_read_registers(3) call

  * **converter** (optional)

    The name of function that should be called to convert register uint16_t value to MQTT UTF-8 value. Format of function name is `plugin_name.function_name`. See converters for details.

  The following examples show how to combine *name*, *register*, *register_type*, and *converter* to output different state values:

  1. single value

    ```
    state:
      name: mqtt_val
      register: net.1.12
      register_type: coil
    ```

  2. unnamed list, each register is polled with a separate modbus_read_registers call

    ```
    state:
      name: mqtt_list
      registers:
        - register: net.1.12
          register_type: input
        - register: net.1.100
          register_type: input
    ```

  3. multiple registers converted to single MQTT value, polled with single modbus_read_registers call

    ```
    state:
      name: mqtt_combined_val
      converter: std.int32
      register: net.1.12
      count: 2
    ```

  4. named list (map)

    ```
    state:
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

*Availability* section extends this default behaviour by defining a single or list of modbus registers that should be read to check if state data is valid. This could be i.e. some fault indicator or hardware switch state.

Configuration values:

  * **name** (required)

    The last part of topic name where availability flag should be published. Full topic name is created as `topic.name/availability.name`

  * **register** (required)

    Modbus register address in the form of <network_name>.<slave_id>.<register_number>
    If `register_number` is a decimal, then first register address is 1.
    If `register_number` is a hexadecimal, then first register address is 0.

    `network_name` and `slave_id` are optional if default values are set for a topic

  * **register_type** (required)

    Modbus register type: coil, input, holding

  * **available_value** (optional, default 1)

    Expected uint16 value read from availability register when availability flag should be set to "1". If other value is read then availability flag is set to "0".

*register*, *register_type* and *available_value* can form a list when multiple registers should be read.

## Data conversion

Data read from modbus registers is by default converted to string and published to MQTT broker.

MQMGateway uses conversion plugins to convert state data read from modbus registers to mqtt value and command mqtt payload to register value, for example to combine multiple modbus registers into single value, use mask to extract one bit, or perform some simple divide operations.

Converter can also be used to convert mqtt command payload to register value.

### Standard converters

Converter functions are defined in libraries dynamically loaded at startup.
MQMGateway contains *std* library with basic converters ready to use:

  * **divide**

    Usage: state, command

    Arguments:
      - divisor (required)
      - precision (optional)
      - low_first (optional)


    Divides modbus value by divisor and rounds to (precision) digits after the decimal.
    Supports int16 in single register and int32 value in two registers.
    For int32 mode the first modbus register holds higher byte, the second holds lower byte if 'low first' is not passed.
    With 'low_first' argument the first modbus register holds lower byte, the second holds higher byte.

  * **int32**

    Usage: state, command

    Arguments:
      - low_first (optional)

    Combines two modbus registers into one 32bit value or writes 32bit mqtt value to two modbus registers.
    Without arguments the first modbus register holds higher byte, the second holds lower byte.
    With 'low_first' argument the first modbus register holds lower byte, the second holds higher byte.


  * **uint32**

    Usage: state, command

    Arguments:
      - low_first (optional)

    Same as int32, but modbus registers are interpreted as unsingned int32.


  * **uint16**

    Usage: state, command

    Parses and writes modbus register data as unsigned int.


  * **bitmask**

    Usage: state

    Arguments:
      - bitmask in hex (default "0xffff")


    Applies a mask to value read from modbus register.

  * **string**

    Usage: state, command

    Parses and writes modbus register data as string.
    Register data is expected as C-Style string in UTF-8 (or ASCII) encoding, e.g. `0x4142` for the string _AB_.
    If there not Nul 0x0 byte at the end, then string size is determined by the number of registers, configured using the `count` setting.

    When writing, converters puts all bytes from Mqtt payload into register bytes. If payload is shorter, then remaining bytes are zeroed.

Converter can be added to modbus register in state and command section.

When state is a single modbus register:

```
  state:
    register: device1.slave2.12
    register_type: input
    converter: std.divide(10,2)
```

When state is combined from multiple modbus registers:

```
  state:
    register: device1.slave2.12
    register_type: input
    count: 2
    converter: std.int32()
```

When mqtt command payload should be converted to register value:

```
    command:
      name: set_val
      register: device1.slave2.12
      register_type: input
      converter: std.divide(10)
```

### Exprtk converter.

Exprtk converter allows to use exprtk expression language to convert register data to mqtt value.
Register values are defined as R0..Rn variables.

  * **evaluate**

    Usage: state

    Arguments:
      - [exprtk expression](http://www.partow.net/programming/exprtk/) with Rx as register variables (required)
      - precision (optional)

    &nbsp;

    The following custom functions for 32-bit numbers are supported in the expression.
    _ABCD_ means a number composed of the byte array `[A, B, C, D]`,
    where _A_ is the most significant byte (MSB) and _D_ is the least-significant byte (LSB).
      - `int32(R0, R1)`:   Cast to signed integer _ABCD_ from `R0` == _AB_ and `R1` == _CD_.
      - `int32(R1, R0)`:   Cast to signed integer _ABCD_ from `R0` == _CD_ and `R1` == _AB_.
      - `uint32(R0, R1)`:  Cast to unsigned integer _ABCD_ from `R0` == _AB_ and `R1` == _CD_.
      - `uint32(R1, R0)`:  Cast to unsigned integer _ABCD_ from `R0` == _CD_ and `R1` == _AB_.
      - `flt32(R0, R1)`:   Cast to float _ABCD_ from `R0` == _AB_ and `R1` == _CD_.
      - `flt32(R1, R0)`:   Cast to float _ABCD_ from `R0` == _CD_ and `R1` == _AB_.
      - `flt32be(R0, R1)`: Cast to float _ABCD_ from `R0` == _BA_ and `R1` == _DC_.
      - `flt32be(R1, R0)`: Cast to float _ABCD_ from `R0` == _DC_ and `R1` == _BA_.

    &nbsp;


    If modbus register contains signed integer data, you can use this cast in the expression:

      - `int16(R0)`: Cast uint16 value from `R0' to int16

#### Examples
Division of two registers with precision 3:

```
  objects:
    - topic: test_state
      state:
        converter: expr.evaluate("R0 / R1", 3)
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
```

Reading the state of a 32-bit float value (byte order `ABCD`) spanning two registers (R0 = `BA`, R1 = `DC`) with precision 3:
```
  objects:
    - topic: test_state
      state:
        converter: expr.evaluate("flt32be(R0, R1)", 3)
        registers:
          - register: tcptest.1.2
            register_type: input
          - register: tcptest.1.3
            register_type: input
```



### Adding custom converters

Custom converters can be added by creating a C++ dynamically loaded library with conversion classes. There is a header only libmodmqttconv that provide base classes for plugin and converter implementations.

Here is a minimal example of custom conversion plugin with help of boost dll library loader:

``` C++

#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT
#include "libmodmqttconv/converterplugin.hpp"

class MyConverter : public DataConverter {
    public:
        //called by modmqttd to set coverter arguments
        virtual void setArgs(const std::vector<std::string>& args) {
            mShift = getIntArg(0, args);
        }

        // Conversion from modbus registers to mqtt value
        // Used when converter is defined for state topic
        // ModbusRegisters contains one register or as many as
        // configured in unnamed register list.
        virtual MqttValue toMqtt(const ModbusRegisters& data) const {
            int val = data.getValue(0);
            return MqttValue::fromInt(val << mShift);
        }

        // Conversion from mqtt value to modbus register data
        // Used when converter is defined for command topic
        virtual ModbusRegisters toModbus(const MqttValue& value, int registerCount) const {
            int val = value.getInt();
            ModbusRegisters ret;
            for (int i = 0; i < registerCount; i++) {
              val = val >> mShift
              ret.prependValue(val);
            }
            return ret;
        }

        virtual ~MyConverter() {}
    private:
      int mShift = 0;
};


class MyPlugin : ConverterPlugin {
    public:
        // name used in configuration as plugin prefix.
        virtual std::string getName() const { return "myplugin"; }
        virtual IStateConverter* getStateConverter(const std::string& name) {
            if (name == "myconverter")
                return new MyConverter();
            return nullptr;
        }
        virtual ~MyPlugin() {}
};

// modmqttd search for "converter_plugin" C symbol in loaded dll
extern "C" BOOST_SYMBOL_EXPORT MyPlugin converter_plugin;
MyPlugin converter_plugin;

```

Compilation on linux:

```
g++ -I<path to mqmgateway source dir> -fPIC -shared myplugin.cpp -o myplugin.so
```


*myconverter* from this example can be used like this:

```
modmqttd:
  loglevel: 5
  converter_search_path:
    - <mylugin.so dir>
  converter_plugins:
    - myplugin.so
mqtt:
  objects:
    - topic: test_topic
    command:
      name: set_val
      register: device1.slave2.12
      register_type: input
      converter: myplugin.myconverter(1)
    state:
      name: test_val
      register: device1.slave2.12
      register_type: input
      converter: myplugin.myconverter(1)

```

For more examples see libstdconv source code.

