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
  build passes `MQM_TEST_TIMING_FACTOR=10`. If a test fails in the full suite but passes in
  isolation, first suspect timing: try increasing the factor up to `10` (the CI ceiling for
  development machines). If failures persist above `10`, the cause is elsewhere.
- `MQM_TEST_LOGLEVEL` — log verbosity during tests (`trace`..`critical`).

CI builds and runs the suite inside `Dockerfile` (Alpine). Three workflows live in
`.github/workflows/`: `main.yml` (push to `master` / release → build **and push** the multi-arch
image to `ghcr.io`), `ci_tests.yml` (push to the **long-lived `ci_tests` branch** → a stripped-down
copy of `main.yml` that is **build & test only** — no registry login, digest upload, or manifest
merge/push (`push=false`), so you can validate CI changes there without publishing), and `lint.yml`
(PRs → diff-scoped
clang-format / clang-tidy naming). Each build is a matrix with a **per-arch runner**: `amd64`/`i386`
on `ubuntu-24.04` (native x86) and `arm64`/`arm/v7`/`arm/v6` on `ubuntu-24.04-arm`. `arm64` builds
natively there; only 32-bit `arm/v7`+`arm/v6` are emulated (gated `Set up QEMU` step, pinned
`tonistiigi/binfmt` image). `fail-fast: false` keeps one flaky arch from cancelling the others.

CI-log gotcha: the GitHub **web** log viewer truncates large buildkit build logs and can appear to
freeze mid-`apk` — it is a rendering limitation, not a stuck build. The authoritative full log is
the gear menu → **"View raw logs"**, or `gh run view <run-id> --log` / `--log-failed`.

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
- **RPC (ad-hoc read/write) path**: an MQTT5 request on `<client_id>/rpc/modbus_request` →
  `handleRpcRequest()` → enqueues a one-shot `MsgRegisterReadRequest` (read) or `MsgRegisterValues`
  (write) → the result is published to the client's Response Topic. See **MQTT5 RPC interface**.

### Publish modes (on_change / every_poll / once)

Each `MqttObject` has a `PublishMode` (`common.hpp`: `ON_CHANGE`, `EVERY_POLL`, `ONCE`), set from
config via `setPublishMode(mode, everyPollRefresh)`. The whole decision of *whether* a poll result
gets published lives in two places — `MqttObject::needStateRepublish()` and
`MqttClient::publishState()` (`mqttclient.cpp`) — and it is **not** obvious from the data flow:

- **`on_change`** (default): `needStateRepublish()` returns `false`. Publication happens *only*
  inside `publishState()`, which compares the freshly generated payload against
  `getLastPublishedPayload()` and skips the MQTT publish when they're equal — genuine value changes
  publish, unchanged re-reads are suppressed (no rate limit needed). Note `refresh` here sets the
  *polling* cadence, i.e. the **worst-case** latency between a register changing and the change being
  published: a change is published *at least* once per `refresh` period. It can be published **more
  frequently** when the register is read for another reason — an RPC read, or a second object polling
  the same register with a shorter `refresh` — because `on_change` publishes as soon as *any* read
  observes a new value.
- **`every_poll`**: `needStateRepublish()` returns `true` once `mEveryPollPeriod` has elapsed since
  the last publish (the period is the object's min register refresh). This republishes even when the
  value is unchanged, but rate-limited to that period. The period gate also lets composite/multi-part
  topics accumulate all their register pieces before emitting (a single poll round can deliver an
  object's registers in several `MsgRegisterValues`).
- **`once`**: published exactly once — `publishState()` early-returns if `getLastPublishTime()` is
  already set.

The `force` argument to `publishState()` bypasses the change-comparison; it's used on
availability transitions (a newly-available object must publish its current state even if the
payload string happens to match the last one). Publication is also gated on availability:
`publishState()` returns early unless the object's `AvailableFlag` is `True`.

### MQTT5 RPC interface

An operator (or AI helper) can read/write **arbitrary** Modbus registers on a running daemon
without editing config, via an MQTT5 request/response exchange. Design reference:
`.claude/plans/quiet-churning-snail.md`.

- **Gate + transport are one switch**: `mqtt.rpc.mode` = `disabled` | `read` | `readwrite`
  (`RpcMode` in `config.hpp`). `disabled` (default) → the connection stays MQTT **3.1.1**, no RPC
  subscription, zero behavioural change. `read`/`readwrite` → the whole broker connection becomes
  **MQTT5** (`MqttBrokerConfig::mProtocolV5`) and the daemon subscribes to
  `<client_id>/rpc/modbus_request`. Protocol version is per-connection, so v3 state/command
  subscribers downstream are unaffected (the broker strips v5-only properties for them).
- **Request/response is MQTT5-native**: the client sets a `Response Topic` and optional
  `Correlation Data` on its PUBLISH; the daemon replies once (never retained) to that Response Topic,
  echoing the Correlation Data. A request with **no Response Topic** can only be logged and dropped.
- **Reply shape is the bare value, not an envelope**: a read reply payload is exactly what a poll
  would publish (scalar for `count==1`, JSON array for `count>1`); a write reply echoes the written
  register values in the same scalar/JSON-array shape, always as **raw uint16** values (no converter
  re-applied on the reply path), optimistically without a device re-read. Errors are carried
  out-of-band in an MQTT5 **`error` User Property** (empty payload). Success ⇒ no `error` property;
  failure ⇒ `error=<message>`.
- **`mCommandId` is the daemon↔modbus correlation tag** — one `int` on `ModbusMessageBase`
  (`modbus_types.hpp`), three-way partitioned: `== 0` scheduled poll (keyed into `mObjects`), `> 0`
  configured write command (keyed into `mCommandObjects`), `< 0` RPC request (keyed into
  `mPendingRpc`, minted by `--mNextRpcId` on the main thread). `hasCommandId()` is `!= 0`,
  `isRpc()` is `< 0`. This is what lets multiple RPC requests be in flight at once.
- **Hot-path safety**: steady-state polls (`mCommandId == 0`) take no RPC branch. `isRpc()` is only
  ever tested *inside* the `hasCommandId()` branch and in the executor/`processModbusMessages` error
  paths, so an arbitrary RPC register that exists in no `MqttObject` never reaches the
  `assert(it != mObjects.end())` on the poll-success path (`mqttclient.cpp`).
- **Poll coordination**: an RPC read becomes a one-shot `RegisterPoll` (refresh `0`,
  `PublishMode::ONCE`) added to the slave's poll queue via `ModbusExecutor::addReadCommand`, electing
  the slave queue if the executor is idle — so it honours `delay_before_command`/retries/watchdog
  like any poll. The RPC read *also* updates the matching polled `MqttObject` if one exists, so
  state-topic subscribers keep seeing values; the object's own publish-mode logic prevents a burst of
  RPC reads from flooding the state topic.

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
- Style and naming are enforced by `.clang-format` and `.clang-tidy` (naming-only). CI gates
  the diff vs master on every PR to master — **only the lines changed vs the merge-base are
  checked**, so pre-existing non-conforming code is never flagged.
- The local targets `format` / `format-check` / `tidy-naming` are **diff-scoped to match CI**:
  they format/check only the lines changed vs `origin/master` (via `git clang-format <merge-base>`
  and `clang-tidy-diff.py`), never the whole tree. All three are thin wrappers over the single
  source of truth `scripts/clang-lint.sh`, which the pre-push hook also calls. So `format` will
  **not** reformat unrelated files, and `tidy-naming` will not flag pre-existing violations dragged
  in by a whole-tree reformat. (Do **not** reintroduce a whole-tree `clang-format -i` — it churns
  ~130 long-committed files and pollutes the branch diff, which then makes the diff-based checks
  fail on pre-existing converter-header violations.) `tidy-naming` ignores
  `clang-diagnostic-error: '…' file not found` notes — those come from clang-tidy parsing a header
  standalone (no compile command → no `-I`), not from a style violation.
- A pre-push hook in `.githooks/pre-push` runs the same two checks before any push to master.
  Activate once per clone: `git config core.hooksPath .githooks`. It needs `build/compile_commands.json`.
- **Always run `cmake --build build --target format-check tidy-naming` before running tests.**
  Lint errors will also be caught by the pre-push hook, but catching them early avoids a
  wasted test cycle.

### TestConfig pattern

Integration-style tests build on `MockedModMqttServerThread`, configured via `TestConfig`
(defined in `unittests/yaml_utils.hpp`). Key rules:

- Define `TestConfig config(R"(...)")` **at TEST_CASE scope**, not inside a SECTION. Catch2
  re-runs the TEST_CASE body from the top for each SECTION, so each section gets a fresh
  `config` object with `mEmitted = false`. Defining it inside a SECTION causes a compile-time
  logic error if `toString()` is ever called twice on the same object.
- To vary a single field across sections, mutate `config.mYAML` before calling `toString()`:
  ```cpp
  config.mYAML["mqtt"]["objects"][0]["state"]["converter"] = "std.float32()";
  MockedModMqttServerThread server(config.toString());
  ```
  Use YAML node path matching the config structure (0-based sequence indices).
- `toString()` is one-shot — calling it twice on the same object throws. It also applies
  timing scaling (`MQM_TEST_TIMING_FACTOR`) before emitting.

### Test timing & synchronization

Time-dependent tests are **not** written with raw sleeps and fixed durations — everything goes
through the `timing` helper (`unittests/timing.{hpp,cpp}`) and condition-variable wait helpers, so
the suite runs correctly whether the machine is fast or a slow ARM CI box.

- **Always express durations via `timing::milliseconds(n)` / `timing::seconds(n)`**, never a raw
  `std::chrono::milliseconds(n)`. Each call multiplies `n` by the global `sFactor`
  (`MQM_TEST_TIMING_FACTOR`; default `10` in `NDEBUG`/release, `1` in debug). A test that hardcodes a
  raw literal for a wait or a configured `refresh` will pass at factor 1 and flake at factor 10.
  `timing::defaultWait` (5s × factor) and `timing::maxTestTime` (10s × factor) are the standard
  timeout constants. `TestConfig::toString()` rewrites the YAML's time values by the same factor, so
  config durations and test-side waits scale together.
- **Don't sleep-then-assert; wait on the event.** The mocks expose blocking helpers that wake on a
  condition variable as soon as the awaited thing happens (or time out): `MockedMqttImpl` —
  `waitForPublish`, `waitForMqttValue`, `waitForSubscription`, `waitForFirstPublish`,
  `waitForRpcResponse`; `MockedModbusContext`/`MockedModbusFactory` — `waitForModbusValue`,
  `waitForInitialPoll`. These are the synchronization primitive; polling with sleeps is both slower
  and flakier.
- **The mocked Modbus context simulates command duration.** Each slave has `mReadTime`/`mWriteTime`
  (`sDefaultSlaveReadTime`/`WriteTime`, overridable per-slave via
  `MockedModbusFactory::setModbusReadTime`/`setModbusWriteTime`) so reads/writes take wall-clock time
  and the scheduler/executor timing logic (delays, batching, watchdog, poll cadence) is exercised
  realistically — not instantaneously. It also records issued read/write calls
  (`getIssuedReadCallsCount`, `getIssuedReadCall`, …) and can inject read/write errors and
  slave/serial disconnects for failure-path tests.
