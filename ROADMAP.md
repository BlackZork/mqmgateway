# Roadmap

Milestones are planned separately and executed one at a time. Each has its own plan document. This
file records what is true today and which milestone is active — not intended edits.

## Current state

`stdconv` provides converters up to 32 bits: `int8`, `uint8`, `int16`, `uint16`, `int32`, `uint32`
and `float32`, alongside `bit`, `bitmask`, `divide`, `multiply`, `scale`, `string`, `map` and
`debug`. There is no 64-bit converter of any kind.

`MqttValue` carries four source types — `INT`, `DOUBLE`, `BINARY` and `INT64`. It has **no unsigned
64-bit holder**, so a value above `INT64_MAX` cannot be represented and would publish as a negative
number. `rpcWriteValueFromJson()` documents this gap in a `TODO` and rejects RPC writes that do not
fit an `int64`.

`exprconv` instantiates exprtk on `double`, so every expression, register helper and result is a
64-bit IEEE float. Integers above 2^53 cannot pass through it exactly.

There is no plugin ABI version constant. The loader resolves the `converter_plugin` symbol with no
handshake, so a third-party converter built against a different header version is not detected.

## Milestone 1 — 64-bit values in `MqttValue`

**Status: in progress.**

Adds `UINT64` and `FLOAT64` source types, collapses the whole-value formatting rule into one
templated `format()` serving both float widths, and introduces a plugin ABI version handshake — the
layout of `MqttValue` changes, and it crosses the plugin boundary by value.

Also carries two prerequisites that everything later depends on: making a converter exception on the
publish path non-fatal (it currently terminates the daemon), and enabling the warning flags that
`libmodmqttsrv`, `modmqttd` and `unittests` are silently built without today.

## Milestone 2 — `std` 64-bit converters

**Status: not started.** Depends on milestone 1.

Adds generic 64-bit register helpers to the public converter API and the converters built on them:
`std.int64`, `std.uint64` and `std.float64`, reading and writing four Modbus registers with the same
`low_first` / `swap_bytes` word- and byte-order arguments as the 32-bit family. Extends `std.debug`
with a four-register section so a 64-bit device register can be identified during commissioning.

Closes [#125](https://github.com/BlackZork/mqmgateway/issues/125).

## Milestone 3 — exprconv `long double` migration

**Status: not started.** Depends on milestone 1.

Widens the exprtk engine from `double` to `long double` so its 64-bit integer helpers are exact, and
gates those helpers out at compile time on platforms whose `long double` is only 53-bit — a config
using them there is rejected at startup rather than publishing a rounded value.

The governing requirement is that `std.uint64` and `expr.evaluate('uint64(R0,R1,R2,R3)')` publish
the same value, or the configuration fails.
