# Roadmap

Milestones are planned separately and executed one at a time. Each has its own plan document. This
file records what is true today and which milestone is active — not intended edits.

## Current state

`stdconv` provides converters up to 32 bits: `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`
and `float32`, alongside `bit`, `bitmask`, `divide`, `multiply`, `scale`, `string`, `map` and
`debug`. There is no 64-bit converter of any kind.

`MqttValue` carries six source types — `INT`, `DOUBLE`, `BINARY`, `INT64`, `UINT64` and `FLOAT64`.
A value up to `UINT64_MAX` is held and published exactly, and `rpcWriteValueFromJson()` accepts an
RPC write of one. Whole-value formatting is decided by one templated `format()` shared by both float
widths, so a `double` and a `long double` holding the same value render identically.

`exprconv` instantiates exprtk on `double`, so every expression, register helper and result is a
64-bit IEEE float. Integers above 2^53 cannot pass through it exactly.

Converter plugins declare `converter_plugin_abi_version`, and `ModMqtt::initConverterPlugin()`
refuses to load a plugin that reports anything but `CONVERTER_ABI_VERSION`, or that omits the marker.
The constant is at 1 and has not been released — it was introduced on this branch, so it still
absorbs further changes to the plugin interface until the branch merges.

A `ConvException` raised while converting a polled value is logged and that one object is skipped;
any other exception still terminates the daemon.

## Milestone 1 — 64-bit values in `MqttValue`

**Status: done.**

Added the `UINT64` and `FLOAT64` source types, collapsed the whole-value formatting rule into one
templated `format()` serving both float widths, and introduced the plugin ABI version handshake —
`MqttValue` crosses the plugin boundary by value, so its changed layout had to become detectable.

Also carried two prerequisites everything later stands on: a converter exception on the publish path
is no longer fatal, and `libmodmqttsrv`, `modmqttd` and `unittests` are built with the warning flags
they were silently missing.

## Milestone 2 — `std` 64-bit converters

**Status: in progress.**

Adds generic 64-bit register helpers to the public converter API and the converters built on them:
`std.int64`, `std.uint64` and `std.float64`, reading and writing four Modbus registers with the same
`low_first` / `swap_bytes` word- and byte-order arguments as the 32-bit family. Extends `std.debug`
with a four-register section so a 64-bit device register can be identified during commissioning.

Closes [#125](https://github.com/BlackZork/mqmgateway/issues/125).

## Milestone 3 — exprconv `long double` migration

**Status: not started.**

Widens the exprtk engine from `double` to `long double` so its 64-bit integer helpers are exact, and
gates those helpers out at compile time on platforms whose `long double` is only 53-bit — a config
using them there is rejected at startup rather than publishing a rounded value.

The governing requirement is that `std.uint64` and `expr.evaluate('uint64(R0,R1,R2,R3)')` publish
the same value, or the configuration fails.
