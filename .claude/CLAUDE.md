# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

`modmqttd` (MQMGateway) is a multithreaded C++17 service that bridges Modbus networks
(TCP/RTU, via libmodbus) and MQTT (via Mosquitto). It polls Modbus registers, converts the
data, and publishes it to MQTT topics; it also subscribes to command topics and writes
converted MQTT payloads back to Modbus registers. Configuration is a single YAML file.

The user-facing config reference is exhaustively documented in `README.md` — consult it for
the meaning of any config key (networks, slaves, objects, state/commands/availability,
poll_groups, publish_mode, retain, converters). Do not duplicate that reference here.

## Build & test

The project uses CMake (Ninja generator) and C++17. Build dependencies: `libmodbus`,
`libmosquitto`, `yaml-cpp`, `RapidJSON`, `spdlog`, `fmt`, `exprtk` (optional, header-only,
located via `find_path`), `Catch2` v3 (optional, for tests).

```bash
cmake -S . -B build              # configure (add -DWITHOUT_TESTS=1 to skip the test target)
cmake --build build              # build everything
cmake --build build --target tests   # build only the unit-test executable
```

- If `exprtk.hpp` is found at configure time, the `exprconv` plugin is built and the tests are
  compiled with `-DHAVE_EXPRTK`. If not found, exprtk-dependent tests are skipped.
- A pre-existing `build/` directory in the repo may be stale (configured from an old source
  path) — reconfigure into a clean directory if `cmake --build` complains about a mismatched
  `CMakeCache.txt`.

### Running tests

Tests are a single Catch2 v3 executable at `build/unittests/tests`. Run it from inside its
own directory — it loads the `stdconv`/`exprconv` plugin `.so`s via relative paths
(`../stdconv`, `../exprconv`), matching `MockedModMqttServerThread`'s converter search paths:

```bash
cd build/unittests && ./tests                  # all tests
./tests "Write single register"                # a single test case by name
./tests "Command topic"                         # name matching (Catch2 supports wildcards)
```

Environment variables that affect tests:
- `MQM_TEST_TIMING_FACTOR` — scales all time-dependent waits/timeouts (see `unittests/timing.cpp`).
  Defaults to `10` in release/`NDEBUG` builds (for slow ARM CI), `1` in debug builds. Raise it
  when debugging timing-sensitive tests (the VS Code launch config uses `100`). The CI Docker
  build passes `MQM_TEST_TIMING_FACTOR=10`.
- `MQM_TEST_LOGLEVEL` — log verbosity during tests (`trace`..`critical`).

CI builds and runs the suite inside `Dockerfile` (Alpine, multi-arch via QEMU). There is no
separate lint step.

### Running the daemon

```bash
modmqttd --config=<path/to/config.yaml> --loglevel=5   # loglevel 1..6, higher = more verbose
```

## Architecture

### Threading model

This is the single most important thing to understand. The process has:

- **One main thread** running the MQTT side: `ModMqtt` (orchestrator) + `MqttClient` + the
  Mosquitto event loop. Mosquitto callbacks (`onMessage`, `onConnect`, …) also fire here.
- **One `ModbusThread` per Modbus network**, each owning its own libmodbus connection and
  doing all blocking Modbus I/O.

Threads communicate only through **per-network lock-free SPSC queues** (moodycamel
`BlockingReaderWriterQueue`, vendored in `readerwriterqueue/`). Each `ModbusClient` holds a
`mToModbusQueue` (main → modbus thread) and `mFromModbusQueue` (modbus thread → main). Queue
items are type-erased `QueueItem`s carrying message structs from `modbus_messages.hpp`
(`MsgRegisterValues`, `MsgRegisterPollSpecification`, `MsgModbusNetworkState`,
`MsgMqttNetworkState`, `EndWorkMessage`, …); dispatch is by `typeid` comparison.

A global `gQueueMutex` + `gHasMessagesCondition` (in `modmqtt.cpp`) let the main loop sleep
until a modbus thread calls `notifyQueues()` after enqueueing a result. Do not add cross-thread
shared state outside this queue+condvar mechanism.

### Data flow

- **Startup**: `ModMqtt::init()` parses YAML → builds `MqttObject`s and one
  `MsgRegisterPollSpecification` per network (the set of registers to poll, with refresh times
  and publish modes), loads converter plugins, then `start()` spins up `MqttClient` and the
  `ModbusClient`/`ModbusThread`s and enters the main message loop (`processModbusMessages()`).
- **Poll (read) path**: `ModbusThread::run()` → `ModbusScheduler` decides which registers are
  due → `ModbusExecutor` issues reads through `IModbusContext` → results sent back as
  `MsgRegisterValues` → `MqttClient::processRegisterValues()` maps them to `MqttObject`s,
  applies state converters, and publishes state + availability topics.
- **Command (write) path**: Mosquitto delivers a message → `MqttClient::onMessage()` finds the
  `MqttObjectCommand`, runs the command converter (MQTT payload → register values) → enqueues
  `MsgRegisterValues` to the network's `ModbusThread` → `ModbusExecutor` performs the write.

### Modbus thread internals (`libmodmqttsrv/`)

- `ModbusScheduler` — holds the poll specification (per-slave register lists) and, given the
  current time, returns the registers due now plus the duration to wait for the next poll.
- `ModbusExecutor` + `ModbusRequestsQueues` — per-slave queues mixing poll reads and write
  commands. Handles inter-command delays (`delay_before_command` /
  `delay_before_first_command`, the latter applied on slave change), read/write retry counts,
  write batching (`WRITE_BATCH_SIZE`), and round-robin between slaves to avoid starvation.
  `executeNext()` either issues one Modbus command (returning duration 0) or returns how long
  to wait before the next command is allowed.
- `ModbusWatchdog` — tracks command error periods and forces a connection restart if no
  command succeeds within `watch_period`; for RTU it also detects an unplugged serial device.
- `RegisterPoll` / `RegisterWrite` (`register_poll.hpp`) — long-lived command objects carrying
  timing, retry, and publish-mode state.

### Hardware abstraction (key for tests)

Two interfaces decouple the logic from real I/O:
- `IModbusContext` / `IModbusFactory` (`imodbuscontext.hpp`) — Modbus transport. Real impl in
  `modbus_context.cpp`; tests use `MockedModbusContext`/`MockedModbusFactory`
  (`unittests/mockedmodbuscontext.*`) which expose in-memory registers, fake read/write
  timings, injectable read errors, and slave/serial disconnects.
- `IMqttImpl` (`imqttimpl.hpp`) — MQTT transport. Real impl `Mosquitto` (`mosquitto.cpp`);
  tests use `MockedMqttImpl`.

Inject mocks via `ModMqtt::setModbusContextFactory()` / `setMqttImplementation()`.

### Converters and plugins

- `libmodmqttconv/` is a **header-only** library defining the public plugin ABI
  (`ConverterPlugin`, `DataConverter`, `MqttValue`, `ModbusRegisters`, `ConverterArgs`). It is
  installed to `include/` and is the interface third-party plugins compile against.
- Converters are loaded at runtime from shared objects (`dlopen`). Each plugin exports a C
  symbol `converter_plugin`. Built-in plugins: `stdconv/` (prefix `std`) and `exprconv/`
  (prefix `expr`, exprtk-based). Config names converters as `pluginname.functionname(args)`.
- A `DataConverter` implements `toMqtt(ModbusRegisters) -> MqttValue` (state/read direction)
  and/or `toModbus(MqttValue, registerCount) -> ModbusRegisters` (command/write direction).
  When no converter is given, `DefaultCommandConverter` handles writes and registers are
  published as their raw uint16 string values.

### Register addressing gotcha

A register address in config is `<network>.<slave>.<register>`. The register number is
**1-based when written in decimal, 0-based when written in hexadecimal**. This convention is
handled in the config/parsing layer — keep it in mind when touching register-id parsing
(`mqtt_register_id_parser_tests.cpp`, `register_address_tests.cpp`).

## Code layout

- `modmqttd/` — the executable: `main.cpp` (CLI arg parsing via `argh/`), default config template.
- `libmodmqttsrv/` — the core static library (`modmqttsrv`): orchestration, MQTT client, modbus
  threads, scheduler/executor/watchdog, config parsing, message types. Almost all logic lives here.
- `libmodmqttconv/` — header-only public converter/plugin ABI (installed).
- `stdconv/`, `exprconv/` — built-in converter plugins (dynamically loaded `.so`s).
- `unittests/` — Catch2 v3 suite; `mockedserver.hpp` (`MockedModMqttServerThread`) spins up a
  full server with mocked Modbus+MQTT and is the harness most integration-style tests build on.
- `readerwriterqueue/`, `argh/` — vendored third-party headers.

## Conventions

- Member variables are prefixed `m` (e.g. `mModbus`, `mSlaves`); arguments often prefixed `p`.
- Logging is spdlog (`logging.hpp`); throw `ConfigurationException` for bad config (tests assert
  on its message via `requireException<>`).
- New unit-test `.cpp` files must be added to the `add_executable(tests ...)` list in
  `unittests/CMakeLists.txt` — there is no globbing.
- Style and naming are enforced by `.clang-format` and `.clang-tidy` (naming-only). Local
  targets: `format` (reformat in-place), `format-check` (dry-run), `tidy-naming` (clang-tidy
  over all `.cpp`). CI gates the diff vs master on every PR to master — only changed lines
  are checked, so pre-existing non-conforming code is never flagged.
- A pre-push hook in `.githooks/pre-push` runs format and naming checks before any push to
  master. Activate once per clone: `git config core.hooksPath .githooks`.
