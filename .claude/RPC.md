# MQTT5 RPC interface for Modbus — implementation reference

This document is the single implementation reference for the `mqtt5_rpc` branch. It supersedes
the planning documents (`mqm-modbus-debug.md`, `quiet-churning-snail.md`) and should be the only
file needed to implement and test the feature.

---

## What we are building

A **gated MQTT5 request/response RPC interface** that lets a client (operator, AI helper, script)
read or write arbitrary Modbus registers on a running `modmqttd` instance — including registers
that are not declared in `config.yaml`.

- **Request topic** (daemon subscribes): `<client_id>/rpc/modbus_request`
- **Reply**: published to the MQTT5 `Response Topic` property set by the client on its PUBLISH
  (retain=false, once). The daemon echoes the client's `Correlation Data` property.
- **RPC-only**: a request without a `Response Topic` is logged and dropped — there is nowhere
  to reply.
- **Gate**: `mqtt.rpc.mode: disabled|read|readwrite`. Disabled = today's behaviour, v3
  connection. Enabled = MQTT5 connection.

---

## Phased delivery

| Phase | Scope | Depends on |
|-------|-------|-----------|
| **1** | `std.debug` converter (standalone) | nothing |
| **2** | RPC mechanism, raw read/write | nothing |
| **3** | Converter integration | 1 + 2 |

Phase 1 ⫫ Phase 2 (build in either order).

---

## Phase 1 — `std.debug` converter

### New file: `stdconv/debug.hpp`

```cpp
class DebugConverter : public DataConverter {
public:
    MqttValue toMqtt(const ModbusRegisters& data) override;  // emits JSON (see shape below)
    ModbusRegisters toModbus(const MqttValue&, int) override; // throws ConvException("std.debug is read-only")
};
```

`toMqtt` builds a JSON object with RapidJSON and returns `MqttValue::fromString(json)` (BINARY).
Reuse `ConverterTools` from `convtools.hpp` (`registersToInt32`, `setByteOrder`,
`toNumber<float>`). **RapidJSON is already on the include path** for `stdconv/` — see
`CMakeLists.txt:45` + `:58`, no CMake change needed.

**JSON shape** emitted by `toMqtt`:

```json
{
  "raw": [16962, 50100],
  "hex": ["0x4242", "0xC3B4"],
  // count==1:
  "int16":  {"normal": 16962, "byteswap": -15934},
  "uint16": {"normal": 16962, "byteswap": 50050},
  // count==2:
  "int32":  {"high_first": {"normal": 1111638964, "byteswap": -1010514941},
              "low_first":  {"normal": -1011417788, "byteswap": 1128481858}},
  "uint32": {"high_first": {"normal": 1111638964, "byteswap": 3284452355}, "low_first": {...}},
  "float32":{"high_first": {"normal": 48.566,     "byteswap": "nan"},      "low_first": {...}},
  // always:
  "string": "BBÃ´"
}
```

Non-finite floats: `std::isfinite` guard; emit `"nan"` / `"inf"` / `"-inf"` as JSON strings.

When used as `converter: std.debug` on a regular object, `MqttPayload::generate` calls
`value.getString()` on the BINARY `MqttValue` and the raw JSON bytes become the state payload.
No special handling needed.

### Edit: `stdconv/plugin.cpp`

Add branch: `else if (name == "debug") return new DebugConverter();`

### Tests (Phase 1)

New cases in existing stdconv test file (or a new `unittests/debug_converter_tests.cpp`, added to
`unittests/CMakeLists.txt`):
- 1-register int16/uint16 normal+byteswap (compute expected with `ConverterTools`)
- 2-register int32/uint32/float32 all 4 word×byte combos
- NaN and Inf → string literals in JSON, output is valid JSON
- `toModbus` throws `ConvException`
- Object with `converter: std.debug` in config publishes JSON doc on its state topic

---

## Phase 2 — RPC mechanism, raw read/write

### 2.1  `mCommandId` unification

**Three-way partition** (the correlation tag, daemon↔Modbus thread):

| `mCommandId` value | meaning | table on main thread | minted at |
|---|---|---|---|
| `== 0` | scheduled poll | `mObjects` (by register ident) | default |
| `> 0` | configured write command | `mCommandObjects[id]` | `nextCommandId=1; nextCommandId++` (`modmqtt.cpp:752`), asserted `>0` at `:237` |
| `< 0` | RPC request/response | `mPendingRpc[id]` | `--mNextRpcId` (main thread only) |

**Property**: `ModbusExecutor`, `RegisterPoll`, `RegisterWrite`, `QueueItem` and the message
structs carry the int as-is; they never inspect the sign. Only the main-thread response handlers
check `< 0`.

#### `libmodmqttsrv/modbus_types.hpp` — `ModbusSlaveAddressRange`

Add three members (keep `mSlaveId` where it is):

```cpp
int  getCommandId() const { return mCommandId; }
bool hasCommandId() const { return mCommandId != 0; }
bool isRpc()        const { return mCommandId < 0; }
int  mCommandId = 0;
```

#### `libmodmqttsrv/mqttcommand.hpp` — `MqttObjectCommand`

- Delete the private `int mCommandId;` field (`:45`) and the `getCommandId()` method (`:43`)
  — both are now inherited from `ModbusSlaveAddressRange`.
- The ctor currently inits `mCommandId` in the member-init-list (`:31`). Move the assignment to
  the **ctor body** because `mCommandId` is now a base member:

  ```cpp
  MqttObjectCommand(...) : ModbusSlaveAddressRange(...), ... { mCommandId = pCommandId; }
  ```

#### `libmodmqttsrv/modbus_messages.hpp`

`MsgRegisterValues`:
- Drop the private `int mCommandId = 0;` (`:39`) and duplicate `getCommandId()/hasCommandId()`
  (`:32-33`) — both inherited.
- The 6-arg ctor (`:18`) inits `mCommandId` in its init-list. Move to ctor body:
  ```cpp
  MsgRegisterValues(..., int pCommandId, ...) : ModbusSlaveAddressRange(...), ... {
      mCommandId = pCommandId;
      ...
  }
  ```

`MsgRegisterReadFailed` and `MsgRegisterWriteFailed` inherit `mCommandId = 0` from the base for
free. No changes needed to their ctors for Phase 2; the executor will set `mCommandId` directly
before sending (see §2.3).

Add the new one-shot read message at the end of `modbus_messages.hpp`:

```cpp
class MsgRegisterReadRequest : public ModbusSlaveAddressRange {
  public:
    MsgRegisterReadRequest(int slaveId, RegisterType t, int reg, int count, int commandId)
        : ModbusSlaveAddressRange(slaveId, reg, t, count) { mCommandId = commandId; }
};
```

> `RegisterPoll` (`register_poll.hpp`) is a *separate* hierarchy (`RegisterCommand →
> ModbusAddressRange`) and is **not** derived from `ModbusSlaveAddressRange`. It gets its own
> `int mCommandId = 0;` field added directly to the class body (`register_poll.hpp:82`).

### 2.2  Config and new types

#### `libmodmqttsrv/config.hpp`

Add the RPC mode enum and a field on `MqttBrokerConfig`:

```cpp
enum class RpcMode { Disabled, Read, ReadWrite };

class MqttBrokerConfig {
    ...
    bool mProtocolV5 = false;   // set true when RpcMode != Disabled
};
```

Add the shared address decode helper (extracted from `RegisterConfigName` in `modmqtt.cpp:80-86`):

```cpp
namespace ConfigTools {
    // 1-based if decimal, 0-based if hex; throws std::invalid_argument on bare decimal 0
    int registerNumberFromString(const std::string& s);
}
```

Call it from both `RegisterConfigName` (replacing the inline logic) and `handleRpcRequest`.

### 2.3  Executor changes — `libmodmqttsrv/modbus_executor.{hpp,cpp}`

#### `addReadCommand` (new method)

Mirrors the *else-branch* of `addWriteCommand` (executor.cpp:139-146) — simple enqueue into
`mPollQueue`, no write-election path (which has the `assert(mWaitingCommand == nullptr)` at `:82`):

```cpp
void ModbusExecutor::addReadCommand(const std::shared_ptr<RegisterPoll>& cmd) {
    ModbusRequestsQueues& q = mSlaveQueues[cmd->mSlaveId];
    q.addPollList({cmd});
    if (mCurrentSlaveQueue == mSlaveQueues.end()) {
        mCurrentSlaveQueue = mSlaveQueues.find(cmd->mSlaveId);
        resetCommandsCounter();
    }
}
```

Declare in `modbus_executor.hpp`.

#### `pollRegisters` — success path (executor.cpp:170)

Add one unconditional assignment after constructing `MsgRegisterValues val(...)`:

```cpp
MsgRegisterValues val(reg.mSlaveId, reg.mRegisterType, reg.mRegister, newValues);
val.mCommandId = reg.mCommandId;   // 0 for normal polls — no branch, no test
sendMessage(QueueItem::create(val));
```

An RPC one-shot `RegisterPoll` has empty `mLastValues`, so `getValues() != newValues` is already
true — no `forceSend` special-case needed.

#### `pollRegisters` — error path (executor.cpp:187-188)

The existing `catch (ModbusReadException)` calls `handleRegisterReadError`. For an RPC read, we
must not invoke that function (it spams the log flood-limiter and emits `MsgRegisterReadFailed`
based on error-count thresholds that don't apply to a one-shot). Replace:

```cpp
} catch (const ModbusReadException& ex) {
    if (reg.mCommandId != 0) {
        reg.mLastReadOk = false;   // sendCommand() will handle retries / terminal failure
    } else {
        handleRegisterReadError(reg, ex.what());
    }
}
```

#### `sendCommand` — RPC terminal failure (executor.cpp:339-373)

In the `RegisterPoll` branch, after `if (!pollcmd.mLastReadOk)` and `mReadRetryCount == 0` (i.e.
retries exhausted, `!retry`), add before `mWaitingCommand.reset()`:

```cpp
if (pollcmd.mCommandId != 0 && mReadRetryCount == 0 && !retry) {
    MsgRegisterReadFailed msg(pollcmd.mSlaveId, pollcmd.mRegisterType,
                              pollcmd.mRegister, pollcmd.getCount());
    msg.mCommandId = pollcmd.mCommandId;
    sendMessage(QueueItem::create(msg));
}
```

#### `writeRegisters` — RPC write failure (executor.cpp:241-249)

After setting `mLastWriteOk = false`, before building `MsgRegisterWriteFailed`:

```cpp
MsgRegisterWriteFailed msg(cmd.mSlaveId, cmd.mRegisterType, cmd.mRegister, cmd.getCount());
if (cmd.mReturnMessage != nullptr)
    msg.mCommandId = cmd.mReturnMessage->getCommandId();
sendMessage(QueueItem::create(msg));
```

This is harmless for normal writes (positive id, but `processRegistersOperationFailed` ignores
it) and enables RPC write failure reporting.

### 2.4  Modbus thread — `libmodmqttsrv/modbus_thread.{hpp,cpp}`

#### `dispatchMessages` (thread.cpp:148)

Add one new branch **after** the `MsgRegisterValues` (write) branch so the more-frequent message
is matched first:

```cpp
} else if (item.isSameAs(typeid(MsgRegisterReadRequest))) {
    processReadRequest(item.getData<MsgRegisterReadRequest>());
}
```

#### `processReadRequest` (new, mirrors `processWrite` at thread.cpp:125)

```cpp
void ModbusThread::processReadRequest(const std::shared_ptr<MsgRegisterReadRequest>& msg) {
    auto cmd = std::make_shared<RegisterPoll>(
        msg->mSlaveId, msg->mRegister, msg->mRegisterType, msg->mCount,
        std::chrono::milliseconds(0), PublishMode::ON_CHANGE
    );
    cmd->mCommandId = msg->mCommandId;   // negative RPC id

    setCommandDelays(*cmd, mDelayBeforeCommand, mDelayBeforeFirstCommand);
    cmd->setMaxRetryCounts(mMaxReadRetryCount, mMaxWriteRetryCount, true);
    auto it = mSlaves.find(msg->mSlaveId);
    if (it != mSlaves.end()) {
        setCommandDelays(*cmd, it->second.getDelayBeforeCommand(),
                               it->second.getDelayBeforeFirstCommand());
        cmd->setMaxRetryCounts(it->second.mMaxReadRetryCount, it->second.mMaxWriteRetryCount);
    }
    mExecutor.addReadCommand(cmd);
}
```

Declare `processReadRequest` in `modbus_thread.hpp`.

### 2.5  Modbus client — `libmodmqttsrv/modbus_client.hpp`

Add alongside `sendCommand` (`:24`):

```cpp
void sendReadRequest(const MsgRegisterReadRequest& req) {
    mToModbusQueue.enqueue(QueueItem::create(req));
}
```

RPC writes reuse the existing `sendCommand` path with a negative `pCommandId`.

### 2.6  MQTT5 transport

#### `libmodmqttsrv/imqttimpl.hpp`

Extend `IMqttImpl`:

```cpp
// Extend existing onMessage signature — existing callers pass nullptr/0 for props
virtual void onMessage(const char* topic, const void* payload, int payloadlen) = 0;  // keep

// Called by handleRpcRequest to send the RPC reply with optional correlation echo
virtual int publishResponse(const char* topic, int len, const void* data,
                            const void* correlationData, int correlationLen) = 0;
```

> Alternatively extend `onMessage` with optional props — see the `Mosquitto::on_message` note
> below. Keep the existing `subscribe` and `publish` signatures untouched.

#### `libmodmqttsrv/mosquitto.hpp`

Add:

```cpp
virtual int publishResponse(const char* topic, int len, const void* data,
                            const void* correlationData, int correlationLen) override;
```

Add the v5 message callback trampoline declaration.

#### `libmodmqttsrv/mosquitto.cpp`

**`Mosquitto::connect`** — when `config.mProtocolV5`, before `mosquitto_connect_async`:

```cpp
if (config.mProtocolV5) {
    mosquitto_int_option(mMosq, MOSQ_OPT_PROTOCOL_VERSION, MQTT_PROTOCOL_V5);
    mosquitto_message_v5_callback_set(mMosq, on_message_v5_wrapper);
} else {
    mosquitto_message_callback_set(mMosq, on_message_wrapper);
}
```

**v5 message wrapper** (new static function):

```cpp
static void on_message_v5_wrapper(struct mosquitto*, void* userdata,
                                  const struct mosquitto_message* message,
                                  const mosquitto_property* props)
{
    char* responseTopic = nullptr;
    void* corrData = nullptr; uint16_t corrLen = 0;

    mosquitto_property_read_string(props, MQTT_PROP_RESPONSE_TOPIC, &responseTopic, false);
    mosquitto_property_read_binary(props, MQTT_PROP_CORRELATION_DATA,
                                   &corrData, &corrLen, false);

    auto* m = static_cast<Mosquitto*>(userdata);
    m->mOwner->onRpcMessage(message->topic, message->payload, message->payloadlen,
                            responseTopic, corrData, corrLen);
    free(responseTopic);
    free(corrData);
}
```

The non-RPC `on_message_wrapper` still calls `mOwner->onMessage(topic, payload, len)` unchanged.

**`Mosquitto::publishResponse`**:

```cpp
int Mosquitto::publishResponse(const char* topic, int len, const void* data,
                               const void* corrData, int corrLen) {
    mosquitto_property* props = nullptr;
    if (corrData && corrLen > 0)
        mosquitto_property_add_binary(&props, MQTT_PROP_CORRELATION_DATA,
                                      corrData, corrLen);
    int msgId;
    int rc = mosquitto_publish_v5(mMosq, &msgId, topic, len, data, 0, false, props);
    mosquitto_property_free_all(&props);
    throwOnCriticalError(rc);
    return msgId;
}
```

### 2.7  `MqttClient` — main-thread side

#### `libmodmqttsrv/mqttclient.hpp` additions

```cpp
// new public
void onRpcMessage(const char* topic, const void* payload, int payloadlen,
                  const char* responseTopic, const void* correlationData, int correlationLen);

// new private helpers
void handleRpcRequest(const void* payload, int payloadlen,
                      const char* responseTopic,
                      const void* correlationData, int correlationLen);
void publishRpcResponse(const std::string& modbusNetworkName,
                        const MsgRegisterValues& values);
void publishRpcError(const std::string& modbusNetworkName,
                     const ModbusSlaveAddressRange& values);

// new private state
std::string mClientId;
std::string mRpcRequestTopic;
RpcMode mRpcMode = RpcMode::Disabled;
int mNextRpcId = 0;

struct PendingRpcRequest {
    std::string network;
    int slaveId;
    int reg;                      // 0-based
    std::string displayAddress;   // original string from the JSON (e.g. "40010")
    RegisterType registerType;
    int count;
    bool isWrite;
    std::shared_ptr<DataConverter> converter;   // nullptr = raw mode (Phase 2: always nullptr)
    std::string responseTopic;
    std::vector<uint8_t> correlationData;
};
std::map<int, PendingRpcRequest> mPendingRpc;

void setRpcMode(RpcMode mode) { mRpcMode = mode; }
```

#### `mqttclient.cpp` — `setClientId` (`:27`)

```cpp
void MqttClient::setClientId(const std::string& clientId) {
    mClientId = clientId;
    mRpcRequestTopic = clientId + "/rpc/modbus_request";
    ...existing...
}
```

#### `mqttclient.cpp` — `onConnect` (`:97`)

After the command subscription loop:

```cpp
if (mRpcMode != RpcMode::Disabled)
    mMqttImpl->subscribe(mRpcRequestTopic.c_str());
```

#### `mqttclient.cpp` — `onMessage` (`:300`)

First line inside `try`, before `findCommand`:

```cpp
// mRpcMode tested first — disabled build short-circuits before the string compare
if (mRpcMode != RpcMode::Disabled && mRpcRequestTopic == topic) {
    // RPC messages arrive via onRpcMessage, not here; ignore on this path
    // (This guard prevents findCommand from throwing ObjectCommandNotFoundException)
    return;
}
```

#### `mqttclient.cpp` — `onRpcMessage` (new)

```cpp
void MqttClient::onRpcMessage(const char* topic, const void* payload, int payloadlen,
                               const char* responseTopic, const void* corrData, int corrLen) {
    if (mRpcMode == RpcMode::Disabled || mRpcRequestTopic != topic)
        return;
    handleRpcRequest(payload, payloadlen, responseTopic, corrData, corrLen);
}
```

#### `mqttclient.cpp` — `handleRpcRequest` (new)

Key logic in order:

1. **Require `responseTopic`** — if null/empty, `spdlog::warn(...)` and return (no error
   envelope — nowhere to send it).
2. **RapidJSON-parse** the payload. On parse failure: `publishRpcError(...)` (build minimal
   envelope and call `mMqttImpl->publishResponse` directly, since no pending entry yet).
3. **Extract fields:**
   - `"network"` (string, required)
   - `"slave"` (int, required)
   - `"register"` (string, required) → `ConfigTools::registerNumberFromString(s)` for 0-based int;
     keep original string as `displayAddress`
   - `"count"` (int, default 1; clamp to libmodbus per-type limits: 125 holding/input, 2000 coil/bit)
   - `"register_type"` (string, default `"holding"`) → call existing `parseRegisterType`
   - `"value"` (array of uint16 for write, or absent)
   - *Phase 2:* `"converter"` field **present** → return error envelope `"converter field not supported yet"`
4. **Determine direction**: `isWrite = value_field_present`.
5. **Mode gate**: write request while `mRpcMode != RpcMode::ReadWrite` →
   `publishRpcError("writes are disabled (mode: read)", ...)`.
6. **Find `ModbusClient`** by network name (same `std::find_if` idiom as `onMessage`/`:306`).
   Not found → `publishRpcError(...)`.
7. **Register-type gate for writes**: `BIT` and `INPUT` are read-only → `publishRpcError(...)`.
8. **Allocate id**: `int id = --mNextRpcId;` (always negative).
9. **Store context**: `mPendingRpc[id] = PendingRpcRequest{..., responseTopic, {corrData, corrData+corrLen}}`.
10. **Enqueue**:
    - Read: `modbusClient->sendReadRequest(MsgRegisterReadRequest{slaveId, regType, reg, count, id})`
    - Write: `modbusClient->sendCommand(MqttObjectCommand{id, ...}, ModbusRegisters(valueArray))`
      — or construct and enqueue a `MsgRegisterValues` directly with the negative id.

#### `mqttclient.cpp` — `processRegisterValues` (`:132`) routing change

Nest the RPC check **inside** the existing `hasCommandId()` branch:

```cpp
if (pSlaveData.hasCommandId()) {
    if (pSlaveData.isRpc()) {
        publishRpcResponse(pModbusNetworkName, pSlaveData);
        return;
    }
    MqttCmdObjMap::iterator it = mCommandObjects.find(pSlaveData.getCommandId());
    if (it != mCommandObjects.end())
        affectedObjects = &(it->second);
} else {
    // normal poll path — UNCHANGED, no new comparison evaluated
    MqttObjectRegisterIdent ident(pModbusNetworkName, pSlaveData);
    MqttPollObjMap::iterator it = mObjects.find(ident);
    assert(it != mObjects.end());
    affectedObjects = &(it->second);
}
```

**Why nested and not hoisted to `processModbusMessages`**: the poll path (`== 0`) takes the
`else` and evaluates **zero** new comparisons. This is a locked hot-path requirement — do not
move the `isRpc()` check up.

Also fixes the `assert(it != mObjects.end())` hazard (`:139`): an RPC register never in
`mObjects` is caught here and never reaches the assert.

#### `mqttclient.cpp` — `publishRpcResponse` (new)

```cpp
void MqttClient::publishRpcResponse(const std::string& net, const MsgRegisterValues& data) {
    auto it = mPendingRpc.find(data.getCommandId());
    if (it == mPendingRpc.end()) return;  // already cleaned up (e.g. disconnect)
    PendingRpcRequest& req = it->second;

    // Phase 2: always emit raw + hex
    // Phase 3: if req.converter != nullptr, apply and embed under "value"
    rapidjson::Document doc;
    doc.SetObject();
    auto& a = doc.GetAllocator();
    doc.AddMember("status", "ok", a);
    doc.AddMember("network", rapidjson::Value(net.c_str(), a), a);
    doc.AddMember("slave", req.slaveId, a);
    doc.AddMember("address", rapidjson::Value(req.displayAddress.c_str(), a), a);
    doc.AddMember("register", req.reg, a);
    doc.AddMember("register_type", rapidjson::Value(registerTypeName(req.registerType), a), a);
    doc.AddMember("count", req.count, a);

    // raw + hex arrays
    rapidjson::Value rawArr(rapidjson::kArrayType);
    rapidjson::Value hexArr(rapidjson::kArrayType);
    for (int i = 0; i < data.mRegisters.getCount(); i++) {
        rawArr.PushBack(data.mRegisters.getValue(i), a);
        char buf[8]; snprintf(buf, sizeof(buf), "0x%04X", data.mRegisters.getValue(i));
        hexArr.PushBack(rapidjson::Value(buf, a), a);
    }
    doc.AddMember("raw", rawArr, a);
    doc.AddMember("hex", hexArr, a);

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    doc.Accept(w);

    mMqttImpl->publishResponse(
        req.responseTopic.c_str(), sb.GetSize(), sb.GetString(),
        req.correlationData.empty() ? nullptr : req.correlationData.data(),
        req.correlationData.size()
    );
    mPendingRpc.erase(it);
}
```

#### `mqttclient.cpp` — `publishRpcError` (new)

Same pattern; set `"status":"error"` and add `"error":"<message>"`. Erase the pending entry.
Called both before a pending entry exists (parse failures) and after (Modbus failures).

#### `mqttclient.cpp` — `onDisconnect`

Add: `mPendingRpc.clear();` — prevents stale lookups after reconnect.

### 2.8  `ModMqtt` changes — `libmodmqttsrv/modmqtt.cpp`

#### `initBroker` (`:341`) — parse `mqtt.rpc.mode`

```cpp
RpcMode rpcMode = RpcMode::Disabled;
if (mqttNode["rpc"]) {
    const YAML::Node& rpc = mqttNode["rpc"];
    std::string modeStr = "disabled";
    ConfigTools::readOptionalValue<std::string>(modeStr, rpc, "mode");
    if      (modeStr == "disabled")  rpcMode = RpcMode::Disabled;
    else if (modeStr == "read")      rpcMode = RpcMode::Read;
    else if (modeStr == "readwrite") rpcMode = RpcMode::ReadWrite;
    else throw ConfigurationException(rpc["mode"].Mark(), "Unknown rpc mode: " + modeStr);
}
mMqtt->setRpcMode(rpcMode);
brokerConfig.mProtocolV5 = (rpcMode != RpcMode::Disabled);
```

#### `processModbusMessages` (`:966`) — RPC failure routing

```cpp
} else if (item.isSameAs(typeid(MsgRegisterReadFailed))) {
    std::unique_ptr<MsgRegisterReadFailed> val(item.getData<MsgRegisterReadFailed>());
    if (val->isRpc())
        mMqtt->publishRpcError((*client)->mNetworkName, *val);
    else
        mMqtt->processRegistersOperationFailed((*client)->mNetworkName, *val);
} else if (item.isSameAs(typeid(MsgRegisterWriteFailed))) {
    std::unique_ptr<MsgRegisterWriteFailed> val(item.getData<MsgRegisterWriteFailed>());
    if (val->isRpc())
        mMqtt->publishRpcError((*client)->mNetworkName, *val);
    else
        mMqtt->processRegistersOperationFailed((*client)->mNetworkName, *val);
```

### 2.9  Config YAML shape

```yaml
mqtt:
  client_id: my_gateway
  rpc:
    mode: disabled        # disabled | read | readwrite  (default: disabled)
  broker:
    host: localhost
    port: 1883
```

When `mode: disabled` (default): daemon connects MQTT 3.1.1, no subscription, all RPC code is
dead. `mode: read` or `mode: readwrite` requires an MQTT5-capable broker (Mosquitto ≥ 1.6).

### 2.10  Mock changes — `unittests/mockedmqttimpl.{hpp,cpp}`

Add:
- `int publishResponse(const char* topic, int len, const void* data, const void* corrData, int corrLen) override` — stores value under `topic` (same as `publish`) AND stores correlation data; provides a test accessor `std::vector<uint8_t> lastCorrelationData(const char* topic)`.
- `void injectRpcRequest(const char* requestTopic, const void* payload, int len, const char* responseTopic, const void* corrData, int corrLen)` — calls `mOwner->onRpcMessage(...)` if the topic is subscribed; useful in tests to fire an RPC request without a real broker.

### 2.11  New test file: `unittests/rpc_modbus_tests.cpp`

Add to `add_executable(tests …)` in `unittests/CMakeLists.txt`.

Minimum test cases for Phase 2:

1. `mode:read` → `waitForSubscription("<id>/rpc/modbus_request")`. `mode:disabled` → not subscribed, broker config `mProtocolV5 == false`.
2. Read via `injectRpcRequest` with Response Topic + Correlation Data → reply on that Response Topic, Correlation Data echoed, `raw`/`hex` present, retain=false.
3. **Request without Response Topic** → logged, no publish, no crash (RPC requirement: response must go somewhere).
4. **Read a register absent from every configured object/poll spec** alongside a normal poll → no assert crash at `mqttclient.cpp:139`, correct payload. *(Locks hot-path safety item #2.)*
5. `mode:readwrite` raw-array write → `getModbusRegisterValue` changed, `status:ok`.
6. `mode:read` write attempt → `status:error`, register unchanged.
7. Network-not-found / bad `register_type` / malformed JSON / `converter` field present → error envelope, no crash.
8. Read failure (`setModbusRegisterReadError`) → error envelope after retries.
9. Write to read-only type (`BIT`, `INPUT`) → clean error, no enqueue.
10. Addressing: decimal `"2"` hits same register as `tcptest.1.2`; hex `"0x1"` is 0-based (crosscheck `register_address_tests.cpp`).
11. **Two requests in flight** (inject two `injectRpcRequest` calls before either response returns, distinct Correlation Data) → each reply carries its own Correlation Data.

---

## Phase 3 — Converter integration

*Only main-thread / MQTT-side changes. No executor or modbus-thread changes.*

### `modmqtt.{hpp,cpp}` — `createConverterFromString`

Extract the body of `createConverter(YAML::Node)` (`:680`) into a **public** method:

```cpp
// modmqtt.hpp (public)
std::shared_ptr<DataConverter> createConverterFromString(const std::string& spec);
```

`createConverter(YAML::Node)` calls it after `IsScalar` check. `MqttClient` calls it via
`mOwner.createConverterFromString(spec)`. Reads only `mConverterPlugins` (immutable after init) →
safe from the Mosquitto callback thread.

### `handleRpcRequest` Phase 3 additions

- `"converter"` field now accepted. Parse the spec string.
- Map: absent or `"none"` / `""` → nullptr (raw mode, Phase 2 shape); else
  `mOwner.createConverterFromString(spec)`. Catch `ConvException`/`MqttObjectException` → `publishRpcError`.
- Store `converter` in `PendingRpcRequest`.
- Write with a converter: encode `"value"` field via `converter->toModbus(mqttVal, count)`;
  fall back to `DefaultCommandConverter` when converter is nullptr.

### `publishRpcResponse` Phase 3 addition

When `req.converter != nullptr`:
- Call `converter->toMqtt(data.mRegisters)` → `MqttValue`.
- Try `rapidjson::Document subDoc; subDoc.Parse(mqttVal.getString())`. If parse succeeds → embed
  sub-document under `"value"` key. If parse fails (scalar) → embed raw string/number.
- Emit the envelope with `"value"` instead of top-level `"raw"`/`"hex"`.

When `req.converter == nullptr` (raw mode, `"none"`) → Phase 2 shape unchanged.

### Phase 3 test additions (in `rpc_modbus_tests.cpp`)

- Default read (no `converter` or `"none"`) → raw shape (Phase 2: `raw`/`hex` arrays, no `"value"` key).
- `"converter":"std.debug"` → `"value"` contains `"int32"`, `"float32"`, `"string"` etc.
- `"converter":"std.float32(low_first=true)"` → `"value"` is a scalar number.
- Converter-encoded write round-trip: write via converter, read back, confirm register value.

---

## RPC message reference

### Request (client → `<client_id>/rpc/modbus_request`, MQTT5 with Response Topic)

```json
{
  "network":       "tcp1",
  "slave":         1,
  "register":      "40010",
  "count":         2,
  "register_type": "holding",
  "converter":     "std.float32(low_first=true)",
  "value":         [1, 2]
}
```

- `register`: string, same convention as `config.yaml` — decimal = 1-based, hex = 0-based (`"0x0009"` and `"40010"` refer to the same register).
- `register_type`: `coil` | `bit` | `holding` | `input` (default `holding`). Writes rejected on `bit`/`input`.
- `count`: default 1; clamped to per-type libmodbus limits (125 holding/input, 2000 coil/bit).
- `converter`: Phase 3 only; omit or `"none"`/`""` → raw (`raw`/`hex` arrays); named converter → output under `"value"`.
- `value`: write only; array of uint16 (raw) or a scalar/JSON value for the named converter (Phase 3).

### Response (daemon → client's Response Topic, Correlation Data echoed)

**Default read (Phase 2 and Phase 3, no `converter` or `converter:"none"`):**
```json
{
  "status": "ok",
  "network": "tcp1", "slave": 1, "address": "40010",
  "register": 16, "register_type": "holding", "count": 2,
  "raw": [16962, 50100],
  "hex": ["0x4242", "0xC3B4"]
}
```

**Read with `converter:"std.debug"` (Phase 3):**
```json
{
  "status": "ok",
  "network": "tcp1", "slave": 1, "address": "40010",
  "register": 16, "register_type": "holding", "count": 2,
  "value": {
    "raw": [16962, 50100], "hex": ["0x4242", "0xC3B4"],
    "int32":   {"high_first": {"normal": 1111638964, "byteswap": -1010514941}, "low_first": {...}},
    "uint32":  {"high_first": {...}, "low_first": {...}},
    "float32": {"high_first": {"normal": 48.566, "byteswap": "nan"}, "low_first": {...}},
    "string":  "BBÃ´"
  }
}
```

**Read with other converter, e.g. `converter:"std.float32(low_first=true)"` (Phase 3):**
```json
{ "status": "ok", ..., "value": 48.566 }
```

**Write success:**
```json
{ "status": "ok", "network": "tcp1", "slave": 1, "register": 16, "register_type": "holding", "count": 2 }
```

**Error:**
```json
{ "status": "error", "error": "writes are disabled (mode: read)", "network": "tcp1", "slave": 1 }
```

---

## Hot-path safety notes

These are invariants to maintain during implementation:

1. **Poll path (`mCommandId == 0`) pays nothing.** The `isRpc()` check lives inside
   `hasCommandId()`. The `else` branch (normal poll) evaluates no new comparison. The only
   change to the poll success path is one unconditional `val.mCommandId = reg.mCommandId`
   assignment (evaluates to `0 = 0` for every normal poll — no branch, no test).

2. **`assert(it != mObjects.end())` in `processRegisterValues` (`:139`)** — an RPC register is
   never in `mObjects`. Mitigated because `isRpc()` returns before reaching the `else`. The test
   case "read absent register alongside normal poll" (test case #4 above) locks this.

3. **`addPollList` election assert (executor.cpp:82)** — never reached because RPC reads use
   `addReadCommand` (the `else` path that just enqueues), not the write-election path.

4. **`pollDone()` interaction** — an RPC read in `mPollQueue` makes `pollDone()` false. If
   injected during the initial poll, the next `addPollList` from `ModbusScheduler` logs
   "Cannot add next registers…" and skips one cycle. Transient, self-healing, non-fatal.

5. **Thread confinement** — `mNextRpcId` and `mPendingRpc` are touched only on the main/Mosquitto
   thread (`handleRpcRequest` ← `onRpcMessage`; `publishRpcResponse/Error` ← `processModbusMessages`).
   Modbus thread only copies the int. No locking needed.

6. **Wraparound** — `--mNextRpcId` at `INT_MIN` after ~2³¹ requests. Ignore; no reset-when-empty
   guard needed.

---

## Files changed per phase

### Phase 1
- `stdconv/debug.hpp` *(new)*
- `stdconv/plugin.cpp` — add `else if (name == "debug")` branch
- `unittests/CMakeLists.txt` — add new test `.cpp` if split out
- `unittests/*debug_converter*_tests.cpp` *(new or appended)*
- `README.md` — document `std.debug` converter

### Phase 2
- `libmodmqttsrv/modbus_types.hpp` — add `mCommandId`/`isRpc()` to `ModbusSlaveAddressRange`
- `libmodmqttsrv/modbus_messages.hpp` — refactor `MsgRegisterValues` id; add `MsgRegisterReadRequest`
- `libmodmqttsrv/mqttcommand.hpp` — unify `MqttObjectCommand` onto the base field
- `libmodmqttsrv/register_poll.hpp` — add `int mCommandId = 0;` to `RegisterPoll`
- `libmodmqttsrv/modbus_executor.{hpp,cpp}` — `addReadCommand`; error branches in `pollRegisters`/`sendCommand`/`writeRegisters`
- `libmodmqttsrv/modbus_thread.{hpp,cpp}` — `dispatchMessages` branch + `processReadRequest`
- `libmodmqttsrv/modbus_client.hpp` — `sendReadRequest`
- `libmodmqttsrv/imqttimpl.hpp` — `publishResponse`; `onRpcMessage` hook (or extend `onMessage`)
- `libmodmqttsrv/mosquitto.{hpp,cpp}` — v5 protocol option, v5 message callback, `publishResponse`
- `libmodmqttsrv/config.hpp` — `RpcMode`, `MqttBrokerConfig::mProtocolV5`, `registerNumberFromString`
- `libmodmqttsrv/mqttclient.{hpp,cpp}` — `mRpcMode`/topic, `onRpcMessage`, `handleRpcRequest`, `publishRpcResponse/Error`, nested `isRpc()` routing, disconnect cleanup
- `libmodmqttsrv/modmqtt.cpp` — `mqtt.rpc.mode` parse, `mProtocolV5`, `isRpc()` failure routing in `processModbusMessages`
- `unittests/mockedmqttimpl.{hpp,cpp}` — `publishResponse`, `injectRpcRequest`, correlation accessors
- `unittests/rpc_modbus_tests.cpp` *(new)* + `unittests/CMakeLists.txt`
- `README.md` — document the RPC interface and config keys

### Phase 3
- `libmodmqttsrv/modmqtt.{hpp,cpp}` — extract `createConverterFromString` (public)
- `libmodmqttsrv/mqttclient.{hpp,cpp}` — `PendingRpcRequest::converter`; converter creation in `handleRpcRequest`; converter apply + JSON embed in `publishRpcResponse`; converter-encoded write
- `unittests/rpc_modbus_tests.cpp` — Phase 3 cases
- `README.md` — converter-in-RPC docs

---

## Verification

### Phase 1
```bash
cmake --build build
cd build/unittests && ./tests "[std.debug]"
```
Also confirm an object with `converter: std.debug` in a dev config publishes valid JSON.

### Phase 2
```bash
cmake --build build
cd build/unittests && ./tests "[rpc]"   # raw suite
./tests                                  # full regression — no poll/command regressions
cmake --build build --target format-check tidy-naming
```
Confirm no `-Wshadow` hit on the unified `mCommandId`.

Manual end-to-end (Mosquitto ≥ 1.6 broker, `mode: read`):
```bash
# Start daemon
modmqttd --config=<cfg> --loglevel=5

# mosquitto_rr sets MQTT5 Response Topic + Correlation Data automatically
mosquitto_rr -h localhost -t '<client_id>/rpc/modbus_request' \
  -e 'responses/me' \
  -m '{"network":"tcp1","slave":1,"register":"40010","count":2}'
# → observe raw+hex JSON on stdout

# Write (mode: readwrite)
mosquitto_rr -h localhost -t '<client_id>/rpc/modbus_request' \
  -e 'responses/me' \
  -m '{"network":"tcp1","slave":1,"register":"40010","value":[1,2]}'
# → status:ok; re-read to confirm

# Write with mode:read → status:error, register unchanged

# mode:disabled → daemon connects v3, not subscribed (check broker v5 handshake logs)
```

### Phase 3
```bash
cd build/unittests && ./tests "[rpc]"   # incl. converter cases
cmake --build build --target format-check tidy-naming
```
Verify `std.debug` output under `"value"` by default; `std.float32` write round-trip.
