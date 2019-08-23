#pragma once

#include <cstdint>
#include <vector>

class ModbusRegisters {
    public:
        ModbusRegisters() {};
        ModbusRegisters(uint16_t value) { addValue(value); }
        int getCount() const { return mRegisters.size(); }
        uint16_t getValue(int idx) const { return mRegisters[idx]; }
        void setValue(int idx, uint16_t val) { mRegisters[idx] = val; }
        void addValue(uint16_t val) { mRegisters.push_back(val); }
    private:
        std::vector<uint16_t> mRegisters;
};
