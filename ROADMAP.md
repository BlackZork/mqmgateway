# Roadmap

Milestones are planned separately and executed one at a time. Each has its own plan document. This
file records what is true today and which milestone is active — not intended edits.

## Current state

`stdconv` provides converters up to 64 bits: `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`,
`float32`, `int64`, `uint64` and `float64`, alongside `bit`, `bitmask`, `divide`, `multiply`,
`scale`, `string`, `map` and `debug`. `ConverterTools` combines and splits registers at any width
through one pair of templates, which the whole family and `exprconv` share.

A converter declares the register count it is designed for, and modmqttd reports a configured
`count` that disagrees once, while the config is read. `std.debug` reports every word and byte order
for one, two and four registers.

`MqttValue` carries six source types — `INT`, `DOUBLE`, `BINARY`, `INT64`, `UINT64` and `FLOAT64`.
A value up to `UINT64_MAX` is held and published exactly, and `rpcWriteValueFromJson()` accepts an
RPC write of one. Whole-value formatting is decided by one templated `format()` shared by both float
widths, so a `double` and a `long double` holding the same value render identically.

`exprconv` instantiates exprtk on `long double`, and offers `int64`, `uint64` and `flt64` register
helpers, each with a `bs` variant, in both directions. A value read from four registers stays exact
from the helper to the published payload, and `std.uint64` and
`expr.evaluate('uint64(R0,R1,R2,R3)')` are checked against each other at converter level, through a
polled topic and through a JSON list. The integer helpers are registered only where a `long double`
has at least 64 mantissa bits, so on 32-bit arm a config naming one is refused at startup with the
reason; the float helpers are offered everywhere, since a `long double` holds an IEEE-754 double
exactly at any width.

Every `write_as` helper narrows through `MqttValue` rather than casting, so a value that does not fit
the target type is reported instead of written. The set of names, the register count each needs and
the writer each uses come from one table.

Converter plugins declare `converter_plugin_abi_version`, and `ModMqtt::initConverterPlugin()`
refuses to load a plugin that reports anything but `CONVERTER_ABI_VERSION`, or that omits the marker.
The constant is at 1 and has not been released — it was introduced on this branch, so it still
absorbs further changes to the plugin interface until the branch merges. `DataConverter` has gained
`getExpectedRegisterCount()` and a protected `requireExpectedRegisterCount()` since it was added.

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

**Status: done.**

Added width-generic register helpers to the public converter API and the converters built on them:
`std.int64`, `std.uint64` and `std.float64`, reading and writing four Modbus registers with the same
`low_first` / `swap_bytes` word- and byte-order arguments as the 32-bit family, which moved onto the
same helpers. Extended `std.debug` with a four-register section so a 64-bit device register can be
identified during commissioning, and made a converter able to declare its width so a mismatched
`count` is reported at startup.

Closed [#125](https://github.com/BlackZork/mqmgateway/issues/125) and
[#127](https://github.com/BlackZork/mqmgateway/issues/127).

## Milestone 3 — exprconv `long double` migration

**Status: done.**

Widened the exprtk engine from `double` to `long double` and added `int64`, `uint64` and `flt64`
register helpers, each with a `bs` variant, for reading and for `write_as`. Word order is spelled by
argument order, as the 32-bit helpers already did, so every `std` word and byte order combination
has an expression that matches it.

Only the integer helpers are gated on the mantissa width. A float helper reads an IEEE-754 double,
which a `long double` holds exactly at any width, so gating those would have refused a configuration
that works; an integer that drifts is usually a bitmask read wrong, which is why those refuse
instead of rounding. The refusal is the absent registration — a gated helper is never added to the
symbol table — and an exprtk-free header explains it after the parse has already failed, where
matching a name that was not the cause costs nothing.

Also carried two fixes the migration reached. `MqttValue` gained `getLongDouble()`, which parses a
command payload with `strtold` rather than `strtod`, since a configured command always arrives as
text and that is where the low bits were being lost. And every `write_as` helper now narrows through
`MqttValue`: each one used to cast a floating point value to an integer before the range check meant
to catch it, so an out of range value was written rather than reported.

## Deferred defects

Open defects observed while working on the milestones but outside their scope. Move each to the
issue tracker when one is reachable — they are recorded here because it was not.

### Re-entrant access to a per-network SPSC queue

Observed 2026-09-05. Running the suite with `MQM_TEST_TIMING_FACTOR=3` aborted in
`mqtt_command_only_tests.cpp`, on the assertion `ReentrantGuard` raises in
`readerwriterqueue/readerwriterqueue.h`:

    Concurrent (or re-entrant) enqueue or dequeue operation detected
    (only one thread at a time may hold the producer or consumer role)

Load dependent, not deterministic: the same test case passed six consecutive runs in isolation at the
same timing factor. It is unrelated to the 64-bit converter work — that test configures no converter
and loads no plugin.

It is worth treating as more than a test artifact. `mToModbusQueue` and `mFromModbusQueue` are single
producer, single consumer by contract, and the whole threading model rests on that; a re-entrancy
report means two threads held one end of a queue, which would be a defect in the daemon or in the
shutdown path rather than in the test. Not investigated further.

### An availability value that does not fit int32 silences the object

`MqttObjectAvailability::getAvailableFlag()` decides availability by comparing the converted value
against `available_value` through `getInt()`. When the converted value does not fit an `int32`,
`getInt()` throws a `ConvException`; `MqttClient` catches it, logs it, and moves on to the next
object. The object publishes nothing — neither its state nor an availability of `0` — and because the
condition comes from the value rather than from a transient fault, every later poll fails the same
way.

Already reachable with `std.uint64()` as an availability converter and a device value above
`INT32_MAX`; the exprconv `long double` migration adds a second route to it, since an expression
result is now range checked where it used to be an unchecked cast.

Deciding what should happen instead is the work: reporting unavailable, comparing at 64-bit width,
or rejecting the configuration at startup are all defensible, and the choice interacts with
[#126](https://github.com/BlackZork/mqmgateway/issues/126), which is the same comparison truncating
to int32. No test asserts the current behaviour, deliberately — pinning it down would cement a
behaviour that has not been chosen.
