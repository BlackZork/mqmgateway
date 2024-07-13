# MQMGateway - MQTT gateway for modbus networks
A multithreaded C++ service that exposes data from multiple [Modbus](http://www.modbus.org/) networks as [MQTT](http://mqtt.org/) topics.

![docker build](https://github.com/BlackZork/mqmgateway/actions/workflows/main.yml/badge.svg)

Main features:
* Connects to multiple TCP and RTU modbus networks
* Handles state and availability for each configured MQTT object
* Allows to read and write to MODBUS registers from MQTT side with custom data conversion
* Flexible MQTT state topic configuration:
  * single modbus register published as string value
  * multiple modbus registers as JSON object
  * multiple modbus registers as JSON list
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
* Out of the box compatibility with [HomeAssistant](https://www.home-assistant.io/integrations/mqtt/) and [OpenHAB](https://www.openhab.org/addons/bindings/mqtt.generic/) interfaces

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

## From sources

1. `git clone https://github.com/BlackZork/mqmgateway.git#branch=master`
   
   You can aslo use `branch=<tagname>` to clone specific release or download sources from [Releases page](https://github.com/BlackZork/mqmgateway/releases)

1. Install dependencies:
   1. boost
   1. libmodbus
   1. mosquitto
   1. yaml-cpp
   1. rapidJSON
   1. exprtk (optional, for exprtk expressions language support in yaml declarations)
   1. Catch2 (optional, for unit tests)

1. Configure and build project:

    ```
    cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -S (project dir) -B (build dir)
    make
    make install
    ```

    You can add -DWITHOUT_TESTS=1 to skip build of unit test executable.

1. Copy config.template.yaml to /etc/modmqttd/config.yaml and adjust it.

1. Copy modmqttd.service to /etc/systemd/system and start service:

```
  systemctl start modmqttd
```

## Docker image

Docker images for various archicetures (i386, arm6, arm7, amd64) are available in [packages section](https://github.com/users/BlackZork/packages/container/package/mqmgateway). 

1. Pull docker image using instructions provided in packages section.

1. Copy config.template.yaml and example [docker-compose file](docker-compose.yml) to working directory

1. Edit and rename config.template.yaml to config.yaml. In docker-compose.yml adjust devices section to provide serial modbus devices from host to docker container.

1. Run `docker-compose up -d` in working directory to start service.

# Configuration

modmqttd configuration file is in YAML format. It is divided into three main sections:

* modmqttd section contains information about custom plugins
* modbus section contains modbus network definitions and slave specific configuration.
* mqtt section contains mqtt broker connection parameters and modbus register mappings to mqtt topics

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

* **response_timeout** (optional, default 500ms)

  A default timeout interval used to wait for modbus response. See modbus_set_response_timeout(3) for details.

* **response_data_timeout** (optional, default 0)

  A default timeout interval used to wait for data when reading response from modbus device. See modbus_set_byte_timeout(3) for details.

* **delay_before_first_command** (timespan, optional, default 0ms)

  Required silence period before issuing first modbus command to a slave. This delay is applied only when gateway switches to a diffrent slave. 

  This option is useful for RTU networks with a mix of very slow and fast responding slaves. Adding a delay before slow slave
  can significantly reduce amount of read erorrs if mqmgateway is configured to poll very fast.

* **delay_before_command** (timespan, optional, default 0ms)

  Same as delay_before_first_command, but a delay is applied to every modbus command sent on this network.

* **read_retries** (optional, default 1)

  A number of retries after a modbus read command fails. A failed command will trigger a publish of "0" value
  to all availability topics for objects which needs command registers for `state` or `command` section.

* **write_retries** (optional, default 2)

  A number of retries after a modbus write command fails.

* **RTU device settings**

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

* **TCP/IP device settings**

  * **address**

    IP address of a device

  * **port**

    TCP port of a device

* **watchdog** (optional)    

  An optional configuration section for modbus connection watchdog. Watchdog monitors modbus command errors. If there is no successful command execution in *watch_period*, then it restarts the modbus connection.

  Additionally for RTU network the *device* path is checked on the first error and then in small (300ms) time periods. If modbus RTU device is unplugged, then connection is restarted.

  * **watch_period** (optional, timespan, default=10s)
    
  The amount of time after which the connection should be reestablished if there has been no successful execution of a modbus command.

* **slaves** (optional)
  An optional slave list with modbus specific configuration like register groups to poll (see poll groups below) and timing constraints

  * **address** (required)

      Modbus slave address. Multiple comma-separated values or ranges are supported like this:
          
          1,2,3-10,12,30
          

  * **name** (optional)

      Name for use in topic `${slave_name}` placeholder.

  * **delay_before_first_command** (timespan, optional)

      Same as global *delay_before_first_command* but applies to this slave only.

  * **delay_before_command** (timespan, optional)

      Same as global *delay_before_command* but applies to this slave only.

  * **response_timeout** (optional)

    Overrides modbus.response_timeout for this slave

  * **response_data_timeout** (optional)

    Overrides modbus.response_data_timeout for this slave

  * **read_retries** (optional)

    A number of retries after a modbus read command to this slave fails. Uses the global *read_retries* if not defined.

  * **write_retries** (optional)

    A number of retries after a modbus write command to this slave fails. Uses the global *write_retries* if not defined.

  * **poll_groups** (optional)

      An optional list of modbus register address ranges that will be polled with a single modbus_read_registers(3) call.

      An example poll group definition to poll 20 INPUT registers at once:

      ```yaml
        poll_groups:
          - register: 1
            register_type: input
            count: 20
      ```

      This definition allows to use single modbus read call to read all data that is needed 
      for multiple topics declared in MQTT section. If there are no topics that use modbus data from 
      a poll group then that poll group is ignored.

      If MQTT topic uses its own register range and this range overlaps a poll group like this:

      ```yaml
        slaves:
          - address: 1
            poll_groups:
              - register: 1
                register_type: input
                count: 20
        […]
        state:
          - name: humidity
            register: 1.18
            register_type: input
            count 5
      ```

      then poll group will be extended to count=23 to issue a single call for reading all data needed for `humidity` topic in single modus read call.

## MQTT section

The mqtt section contains broker definition and modbus register mappings. Mappings describe how modbus data should be published as mqtt topics.

* **client_id** (required)

  name used to connect to mqtt broker.

* **refresh** (timespan, optional, default 5s)

  A timespan used to poll modbus registers. This setting is propagated
  down to an object and register definitions. If this value is less than the target network can handle
  then newly scheduled read commands will be merged with those already in modbus command queue.

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
    - *availability* - for checking if modbus data is available.

  Topic can contain placeholders for modbus network and slave properies in form `${placeholder_name}`. Following placeholders are supported:

    - *slave_address* - replaced by modbus slave address value
    - *slave_name* - replaced by name set in modbus.slaves section
    - *network_name* - replaced by modbus network name

### Topic default values:

  * **refresh** (optional)

    Overrides mqtt.refresh for all state and availability sections in this topic

  * **network** (optional)

    Sets default network name for all state, commands and availability sections in this topic.

    Multiple comma-separated values are supported. If more than one value is set, then you have to include `${network}` placeholder in `topic` value.

  * **slave** (optional)

    Sets default modbus slave address for all state, commands and availability sections in this topic.

    Multiple values are supported as comma-separated list of numbers or number ranges like this:

        1,2,3,5-18,21

    If more than one value is set, then you have to include `${slave_address}` or `${slave_name}` in `topic` value.

    For examples see [Multi-device definitions](#multi-device-definitions) section.

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

  ```yaml
  objects:
  - topic: test_switch
    commands:
      - name: set
        register: tcptest.1.2
        register_type: holding
        converter: std.divide(100)
  ```

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

  ```yaml
  state:
    name: mqtt_combined_val
    converter: std.int32
    register: net.1.12
    count: 2
  ```

  This declaration creates a poll group. Poll group is read from modbus slave using a single
  modbus_read_registers(3) call. Overlapping poll groups are merged with each other and with
  poll groups defined in modbus section.

  2. as list of registers:

  ```yaml
  state:
    - name: humidity
      register: net.1.12
      register_type: input
      # optional
      converter: std.divide(100,2)
    - name:  temp1
      register: net.1.300
      register_type: input
  ```

  This declaration do not create a poll group, but allows to construct MQTT topic data
  from different slaves, even on different modbus networks. On exception is if there are poll groups defined in modbus section, that overlaps state register definitions. In this case
  data is polled using poll group.

  * **refresh**

    Overrides mqtt.refresh for this state topic


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

  * **count** (optional, default: 1)

     If defined, then this describes register range to poll. Register range is always
     polled with a single modbus_read_registers(3) call

  * **converter** (optional)

    The name of function that should be called to convert register uint16_t value to MQTT UTF-8 value. Format of function name is `plugin_name.function_name`. See converters for details.

  The following examples show how to combine *name*, *register*, *register_type*, and *converter* to output different state values:

  1. single value

  ```yaml
  state:
    name: mqtt_val
    register: net.1.12
    register_type: coil
  ```

  2. unnamed list, each register is polled with a separate modbus_read_registers call

  ```yaml
  state:
    name: mqtt_list
    registers:
      - register: net.1.12
        register_type: input
      - register: net.1.100
        register_type: input
  ```

  3. multiple registers converted to single MQTT value, polled with single modbus_read_registers call

  ```yaml
  state:
    name: mqtt_combined_val
    converter: std.int32
    register: net.1.12
    count: 2
  ```

  4. named list (map)

  ```yaml
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

In all of above examples *refresh* can be added at any level to set different values to
whole list or a single register.

Lists and maps can be nested if needed:

```yaml
state:
  - name: humidity
    register: net.1.12
    count: 2
    converter: std.float()
  - name: other_params
    registers:
      - name: "temp1"
        register: net.1.14
        count: 2
        converter: std.int32()
      - name: "temp2"
        register: net.1.16
        count: 2
        converter: std.int32()
```

MQTT output: `{"humidity": 32.45, "other_params": { "temp1": 23, "temp2": 66 }}`

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

  * **count**

     If defined, then this describes register range to poll. Register range is always
     polled with a single modbus_read_registers(3) call

  * **converter** (optional)

    The name of function that should be called to convert register uint16_t values to MQTT UTF-8 value. Format of function name is `plugin_name.function_name`. See converters for details.
    After conversion, MQTT value is compared to available_value. "1" is published if values are equal, 
    otherwise "0".

  * **available_value** (optional, default 1)

    Expected MQTT value read from availability register (or list of registers passed to converter) when availability flag should be set to "1". If other value is read then availability flag is set to "0".

*register*, *register_type* can form a **registers:** list when multiple registers should be read. In this case converter is mandatory and no nesting is allowed. See examples in state section.

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


    Divides value by divisor and rounds to (precision) digits after the decimal.
    For modbus data supports uint16 in single register and uint32 value in two registers.
    For int32 mode the first modbus register holds higher byte, the second holds lower byte if 'low first' is not passed.
    With 'low_first' argument the first modbus register holds lower byte, the second holds higher byte.

  * **multiply**

    Usage: state, command

    Arguments:
      - multipler (required)
      - precision (optional)
      - low_first (optional)

    Multiples value. See 'divide' for description of 'precision' and 'low_first' arguments.

  * **int8**

    Usage: state

    Arguments:
     - first (optional)

    Parses and writes modbus register data as signed int8. Second byte is parsed by default, pass `first` as argument to read first byte.


  * **uint8**

    Usage: state

    Arguments:
     - first (optional)

    Parses and writes modbus register data as unsigned int8. Second byte is parsed by default, pass `first` as argument to read first byte.


  * **int16**

    Usage: state, command

    Parses and writes modbus register data as signed int16.


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

  * **float32**

    Usage: state, command
    Arguments:
      - precision (optional)
      - low_first, high_first (optional)
      - swap_bytes (optional)

    Combines two modbus registers into one 32bit float or writes mqtt value to two modbus registers as float.
    Without arguments the first modbus register holds higher byte, the second holds lower byte.
    With 'low_first' argument the first modbus register holds lower byte, the second holds higher byte.

    If 'swap_bytes' is defined, then bytes in both registers are swapped before reading and writing. Float value stored on four bytes _ABCD_ will be written to modbus registers R0, R1 as:

    - no arguments or high_first: R0=_AB_, R1=_CD_
    - low_first: R0=_CD_, R1=_AB_
    - high_first, swap_bytes: R0=_BA_, R1=_DC_
    - low_first, swap_bytes: R0=_DC_, R1=_BA_

  * **bitmask**

    Usage: state

    Arguments:
      - bitmask in hex (default "0xffff")


    Applies a mask to value read from modbus register.


  * **bit**

    Usage: state (single holding or input register)

    Arguments:
      - bit number 1-16

    Returns 1 if given bit is set, 0 otherwise


  * **string**

    Usage: state, command

    Parses and writes modbus register data as string.
    Register data is expected as C-Style string in UTF-8 (or ASCII) encoding, e.g. `0x4142` for the string _AB_.
    If there not Nul 0x0 byte at the end, then string size is determined by the number of registers, configured using the `count` setting.

    When writing, converters puts all bytes from Mqtt payload into register bytes. If payload is shorter, then remaining bytes are zeroed.

Converter can be added to modbus register in state and command section.

When state is a single modbus register:

```yaml
  state:
    register: device1.slave2.12
    register_type: input
    converter: std.divide(10,2)
```

When state is combined from multiple modbus registers:

```yaml
  state:
    register: device1.slave2.12
    register_type: input
    count: 2
    converter: std.int32()
```

When mqtt command payload should be converted to register value:

```yaml
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

```yaml
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

```yaml
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

```C++

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

```yaml
modmqttd:
  converter_search_path:
    - <myplugin.so dir>
  converter_plugins:
    - myplugin.so
modbus:
  networks:
    - name: tcptest
      address: localhost
      port: 501
mqtt:
  objects:
    - topic: test_topic
    command:
      name: set_val
      register: tcptest.2.12
      register_type: input
      converter: myplugin.myconverter(1)
    state:
      name: test_val
      register: tcptest.2.12
      register_type: input
      converter: myplugin.myconverter(1)

```

For more examples see libstdconv source code.

## Multi-device definitions

Multi-device defintions allows to set slave properties or create a single topic for multiple modbus devices of the same type. This greatly reduces the number of configuration sections that differ only by slave address or modbus network name.

### Multi-device MQTT topics 

If there are many devices of the same type then a MQTT topic for group of devices can be defined by setting `slave` value to list of modbus slave addresses. Then you have to add either `slave_name` or `slave_address` placeholder in the topic string like this:

```yaml
modbus:
  networks:
    - name: basement
      slaves:
        - address: 1
          name: meter1 
        - address: 2
          name: meter2 
        [...]
mqtt:
  objects:
    - topic: ${network}/${slave_name}/node_${slave_address}
      slave: 1,2,3,8-9
      network: basement, roof
      state:
        register: 1
```

In the above example 10 registers will be polled and their values will be published as `/basement/meter1/node_1/state`, `/basement/meter2/node_2/state` and so on.

Slave names are required only if `${slave_name}` placeholder is used.

### Multi device modbus slave definitions.

Address list can be used in `modbus.networks.slaves.address` to define properties for multiple slaves at once:

```yaml
modbus:
  networks:
    - name: basement
      slaves:
        - address: 1,2,3,5-18
          poll_groups:
            - register: 3
              count: 10
            - register: 30
              count: 5
```

A single slave address can be listed in multiple entries like this:

```yaml
modbus:
  networks:
    - name: basement
      slaves:
        - address: 1
          name: meter1 
          response_timeout: 50ms
        - address: 2
          name: meter2 
        - address: 1,2
          response_timeout: 100ms
          poll_groups:
            - register: 3
              count: 10
```

This example will set response timeout 100ms for both slaves - overriding value for slave1. Two poll groups are defined for reading registers 3-13 for both slaves.
