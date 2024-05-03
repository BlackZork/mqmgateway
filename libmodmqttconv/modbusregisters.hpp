#pragma once

#include <cstdint>
#include <vector>
#include <cassert>

class ModbusRegisters {
    public:
        ModbusRegisters() {};
        ModbusRegisters(uint16_t value) { appendValue(value); }
        ModbusRegisters(const std::vector<uint16_t>& values) : mRegisters(values) {}
        int getCount() const { return mRegisters.size(); }
        uint16_t getValue(int idx) const { return mRegisters[idx]; }
        void setValue(uint32_t idx, uint16_t val) {
#ifndef NDEBUG
            assert(idx < mRegisters.size());
#endif
            mRegisters[idx] = val;
        }
        const std::vector<uint16_t>& values() const { return mRegisters; }

        [[deprecated]]
        void addValue(uint16_t val) { appendValue(val); }

        void appendValue(uint16_t val) { mRegisters.push_back(val); }
        void prependValue(uint16_t val) { mRegisters.insert(mRegisters.begin(), val); }
    private:
        std::vector<uint16_t> mRegisters;
};
