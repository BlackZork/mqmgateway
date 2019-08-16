# modbusmqttgw - MQTT gateway for modbus networks

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
* Fast modbus frequency polling, configurable per newtork and per register

modbusmqttgw depends on [libmodbus](https://libmodbus.org/) and [Mosqutto](https://mosquitto.org/) MQTT library. It is developed under Linux, but it should be easy to port it to other platforms.

This software is dual licensed. 

# Installation



# License

This software is dual-licensed:
  * under the terms of [AGPL-3.0 license](https://www.gnu.org/licenses/agpl-3.0.html) as Open Source project.
  * under commercial license.

For a commercial-friendly license and support please see https://modmqttgw.zork.pl. You can find the commercial license terms in COMLICENSE.


