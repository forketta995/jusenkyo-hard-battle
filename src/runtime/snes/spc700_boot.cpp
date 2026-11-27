#include "snes_apu.h"

namespace {

constexpr uint8_t carry = 0x01;
constexpr uint8_t zero = 0x02;
constexpr uint8_t half_carry = 0x08;
constexpr uint8_t page = 0x20;
constexpr uint8_t overflow = 0x40;
constexpr uint8_t negative = 0x80;
struct PortReadPending {};

uint8_t read(SnesApu &apu, uint16_t address) {
    if (address >= 0xF4 && address <= 0xF7) {
        return apu.from_cpu[address - 0xF4];
    }
    switch (address) {
    case 0xF2: return apu.dsp_address;
    case 0xF3: return apu.dsp[apu.dsp_address & 0x7F];
    case 0xFD:
    case 0xFE:
    case 0xFF: return 0; // Timers have no elapsed ticks during driver initialization.
    default: return apu.ram[address];
    }
}

void write(SnesApu &apu, uint16_t address, uint8_t value) {
    apu.ram[address] = value;
    if (address >= 0xF4 && address <= 0xF7) {
        apu.to_cpu[address - 0xF4] = value;
        if (address == 0xF4 && apu.command_pending && value == apu.from_cpu[0]) {
            apu.command_pending = false;
            ++apu.acknowledged_packets;
        }
    } else if (address >= 0xFA && address <= 0xFC) {
        apu.timer_targets[address - 0xFA] = value;
    } else if (address == 0xF1) {
        apu.control = value;
        if (value & 0x10) {
            apu.from_cpu[0] = apu.from_cpu[1] = 0;
        }
        if (value & 0x20) {
            apu.from_cpu[2] = apu.from_cpu[3] = 0;
        }
    } else if (address == 0xF2) {
        apu.dsp_address = value;
    } else if (address == 0xF3 && apu.dsp_address < 0x80) {
        apu.dsp[apu.dsp_address] = apu.dsp_address == 0x7C ? 0 : value;
    }
}

class Spc700 {
public:
    explicit Spc700(SnesApu &apu) : apu_(apu), cpu_(apu.driver) {}

    bool step() {
        const auto address = cpu_.pc;
        const auto opcode = fetch();
        switch (opcode) {
        case 0x02: case 0x22: case 0x42: case 0x62:
        case 0x82: case 0xA2: case 0xC2: case 0xE2:
        case 0x12: case 0x32: case 0x52: case 0x72:
        case 0x92: case 0xB2: case 0xD2: case 0xF2:
            modify_bit(opcode >> 5, !(opcode & 0x10));
            break;
        case 0x04: load(cpu_.accumulator, cpu_.accumulator | read_bus(direct(fetch()))); break;
        case 0x08: load(cpu_.accumulator, cpu_.accumulator | fetch()); break;
        case 0x0B: {
            const auto target = direct(fetch());
            write(apu_, target, shift_left(read_bus(target)));
            break;
        }
        case 0x10: branch(!(cpu_.status & negative)); break;
        case 0x1A: modify_direct_word(fetch(), -1); break;
        case 0x1C: cpu_.accumulator = shift_left(cpu_.accumulator); break;
        case 0x1F: {
            const uint16_t pointer = fetch_word() + cpu_.x;
            const auto low = read_bus(pointer);
            cpu_.pc = low | (read_bus(pointer + 1) << 8);
            break;
        }
        case 0x20: cpu_.status &= ~page; break;
        case 0x28: load(cpu_.accumulator, cpu_.accumulator & fetch()); break;
        case 0x2D: push(cpu_.accumulator); break;
        case 0x2F: branch(true); break;
        case 0x30: branch(cpu_.status & negative); break;
        case 0x3A: modify_direct_word(fetch(), 1); break;
        case 0x3D: load(cpu_.x, cpu_.x + 1); break;
        case 0x3E: compare(cpu_.x, read_bus(direct(fetch()))); break;
        case 0x3F: {
            const auto target = fetch_word();
            push(cpu_.pc >> 8);
            push(cpu_.pc);
            cpu_.pc = target;
            break;
        }
        case 0x48: load(cpu_.accumulator, cpu_.accumulator ^ fetch()); break;
        case 0x4D: push(cpu_.x); break;
        case 0x5D: load(cpu_.x, cpu_.accumulator); break;
        case 0x5F: cpu_.pc = fetch_word(); break;
        case 0x60: cpu_.status &= ~carry; break;
        case 0x64: compare(cpu_.accumulator, read_bus(direct(fetch()))); break;
        case 0x68: compare(cpu_.accumulator, fetch()); break;
        case 0x6B: {
            const auto target = direct(fetch());
            const auto original = read_bus(target);
            const unsigned incoming = (cpu_.status & carry) << 7;
            cpu_.status = (cpu_.status & ~carry) | (original & 1);
            uint8_t result = 0;
            load(result, (original >> 1) | incoming);
            write(apu_, target, result);
            break;
        }
        case 0x6D: push(cpu_.y); break;
        case 0x6F: {
            const uint16_t low = pull();
            cpu_.pc = low | (pull() << 8);
            break;
        }
        case 0x78: {
            const auto operand = fetch();
            compare(read_bus(direct(fetch())), operand);
            break;
        }
        case 0x7D: load(cpu_.accumulator, cpu_.x); break;
        case 0x80: cpu_.status |= carry; break;
        case 0x84: add(read_bus(direct(fetch()))); break;
        case 0x88: add(fetch()); break;
        case 0x8D: load(cpu_.y, fetch()); break;
        case 0x8F: {
            const auto value = fetch();
            write(apu_, direct(fetch()), value);
            break;
        }
        case 0x90: branch(!(cpu_.status & carry)); break;
        case 0x9A: subtract_word(read_direct_word(fetch())); break;
        case 0x9C: load(cpu_.accumulator, cpu_.accumulator - 1); break;
        case 0x9E: divide(); break;
        case 0xA5: add(static_cast<uint8_t>(~read_bus(fetch_word()))); break;
        case 0xAB: {
            const auto target = direct(fetch());
            uint8_t value = 0;
            load(value, read_bus(target) + 1);
            write(apu_, target, value);
            break;
        }
        case 0xAD: compare(cpu_.y, fetch()); break;
        case 0xAE: cpu_.accumulator = pull(); break;
        case 0xAF: write(apu_, direct(cpu_.x++), cpu_.accumulator); break;
        case 0xB0: branch(cpu_.status & carry); break;
        case 0xBA: {
            const auto value = read_direct_word(fetch());
            cpu_.accumulator = value;
            load(cpu_.y, value >> 8);
            cpu_.status = (cpu_.status & ~zero) | (value == 0 ? zero : 0);
            break;
        }
        case 0xBC: load(cpu_.accumulator, cpu_.accumulator + 1); break;
        case 0xBD: cpu_.stack = cpu_.x; break;
        case 0xC4: write(apu_, direct(fetch()), cpu_.accumulator); break;
        case 0xC5: write(apu_, fetch_word(), cpu_.accumulator); break;
        case 0xC8: compare(cpu_.x, fetch()); break;
        case 0xC9: write(apu_, fetch_word(), cpu_.x); break;
        case 0xCB: write(apu_, direct(fetch()), cpu_.y); break;
        case 0xCD: load(cpu_.x, fetch()); break;
        case 0xCE: cpu_.x = pull(); break;
        case 0xD0: branch(!(cpu_.status & zero)); break;
        case 0xD4: write(apu_, direct(fetch() + cpu_.x), cpu_.accumulator); break;
        case 0xD5: write(apu_, fetch_word() + cpu_.x, cpu_.accumulator); break;
        case 0xD6: write(apu_, fetch_word() + cpu_.y, cpu_.accumulator); break;
        case 0xD7: {
            const auto offset = fetch();
            const auto target = read_bus(direct(offset)) |
                                (read_bus(direct(offset + 1)) << 8);
            write(apu_, target + cpu_.y, cpu_.accumulator);
            break;
        }
        case 0xD8: write(apu_, direct(fetch()), cpu_.x); break;
        case 0xDA: {
            const auto offset = fetch();
            write(apu_, direct(offset), cpu_.accumulator);
            write(apu_, direct(offset + 1), cpu_.y);
            break;
        }
        case 0xDC: load(cpu_.y, cpu_.y - 1); break;
        case 0xDD: load(cpu_.accumulator, cpu_.y); break;
        case 0xE4: load(cpu_.accumulator, read_bus(direct(fetch()))); break;
        case 0xE5: load(cpu_.accumulator, read_bus(fetch_word())); break;
        case 0xE8: load(cpu_.accumulator, fetch()); break;
        case 0xEB: load(cpu_.y, read_bus(direct(fetch()))); break;
        case 0xED: cpu_.status ^= carry; break;
        case 0xEE: cpu_.y = pull(); break;
        case 0xF0: branch(cpu_.status & zero); break;
        case 0xF3: branch((read_bus(direct(fetch())) & 0x80) == 0); break;
        case 0xF4: load(cpu_.accumulator, read_bus(direct(fetch() + cpu_.x))); break;
        case 0xF5: load(cpu_.accumulator, read_bus(fetch_word() + cpu_.x)); break;
        case 0xF6: load(cpu_.accumulator, read_bus(fetch_word() + cpu_.y)); break;
        case 0xF7: load(cpu_.accumulator, read_bus(read_direct_word(fetch()) + cpu_.y)); break;
        case 0xF8: load(cpu_.x, read_bus(direct(fetch()))); break;
        case 0xFC: load(cpu_.y, cpu_.y + 1); break;
        case 0xFD: load(cpu_.y, cpu_.accumulator); break;
        case 0xFE: branch(--cpu_.y != 0); break;
        default:
            cpu_.pc = address;
            apu_.driver_opcode = opcode;
            return false;
        }
        return true;
    }

    bool finish_reply() {
        stop_at_port_read_ = true;
        for (unsigned instruction = 0; instruction < 500000; ++instruction) {
            const auto previous = cpu_;
            try {
                if (!step()) {
                    return false;
                }
            } catch (const PortReadPending &) {
                cpu_ = previous;
                return true;
            }
        }
        return false;
    }

    bool resume_from_port_read() {
        stop_at_port_read_ = false;
        return step();
    }

private:
    SnesApu &apu_;
    Spc700State &cpu_;
    bool stop_at_port_read_ = false;

    uint8_t read_bus(uint16_t address) {
        if (stop_at_port_read_ && address == 0xF4) {
            throw PortReadPending{};
        }
        return read(apu_, address);
    }

    uint8_t fetch() { return read_bus(cpu_.pc++); }

    uint16_t fetch_word() {
        const uint16_t low = fetch();
        return low | (fetch() << 8);
    }

    uint16_t direct(uint8_t offset) const {
        return (cpu_.status & page ? 0x100 : 0) | offset;
    }

    uint16_t read_direct_word(uint8_t offset) {
        const uint16_t low = read_bus(direct(offset));
        return low | (read_bus(direct(offset + 1)) << 8);
    }

    void modify_direct_word(uint8_t offset, int change) {
        const uint16_t value = read_direct_word(offset) + change;
        write(apu_, direct(offset), value);
        write(apu_, direct(offset + 1), value >> 8);
        cpu_.status = (cpu_.status & ~(negative | zero)) |
                      ((value >> 8) & negative) | (value == 0 ? zero : 0);
    }

    void push(uint8_t value) { write(apu_, 0x100 | cpu_.stack--, value); }
    uint8_t pull() { return read_bus(0x100 | ++cpu_.stack); }

    void load(uint8_t &target, uint8_t value) {
        target = value;
        cpu_.status = (cpu_.status & ~(negative | zero)) |
                      (value & negative) | (value == 0 ? zero : 0);
    }

    void compare(uint8_t left, uint8_t right) {
        cpu_.status = (cpu_.status & ~carry) | (left >= right ? carry : 0);
        uint8_t difference = 0;
        load(difference, left - right);
    }

    uint8_t shift_left(uint8_t value) {
        cpu_.status = (cpu_.status & ~carry) | (value >> 7);
        uint8_t result = 0;
        load(result, value << 1);
        return result;
    }

    void add(uint8_t value) {
        const auto left = cpu_.accumulator;
        const unsigned carry_in = cpu_.status & carry;
        const unsigned sum = left + value + carry_in;
        cpu_.status &= ~(carry | half_carry | overflow);
        if (sum > 0xFF) cpu_.status |= carry;
        if ((left & 0xF) + (value & 0xF) + carry_in > 0xF) cpu_.status |= half_carry;
        if ((~(left ^ value) & (left ^ sum) & 0x80) != 0) cpu_.status |= overflow;
        load(cpu_.accumulator, sum);
    }

    void divide() {
        const unsigned divisor = cpu_.x;
        const unsigned high = cpu_.y;
        const unsigned dividend = (high << 8) | cpu_.accumulator;
        cpu_.status &= ~(overflow | half_carry);
        if ((high & 0xF) >= (divisor & 0xF)) cpu_.status |= half_carry;
        if (high >= divisor) cpu_.status |= overflow;
        if (high < divisor * 2) {
            cpu_.y = dividend % divisor;
            load(cpu_.accumulator, dividend / divisor);
        } else {
            // The SPC700 uses a different quotient/remainder path on overflow, including X=0.
            const unsigned adjusted = dividend - divisor * 512;
            cpu_.y = divisor + adjusted % (256 - divisor);
            load(cpu_.accumulator, 255 - adjusted / (256 - divisor));
        }
    }

    void subtract_word(uint16_t value) {
        const unsigned left = (cpu_.y << 8) | cpu_.accumulator;
        const uint16_t result = left - value;
        cpu_.status &= ~(carry | zero | half_carry | overflow | negative);
        if (left >= value) cpu_.status |= carry;
        if ((left & 0x0FFF) >= (value & 0x0FFF)) cpu_.status |= half_carry;
        if (((left ^ value) & (left ^ result) & 0x8000) != 0) cpu_.status |= overflow;
        if (result == 0) cpu_.status |= zero;
        cpu_.status |= (result >> 8) & negative;
        cpu_.accumulator = result;
        cpu_.y = result >> 8;
    }

    void branch(bool take) {
        const int8_t offset = static_cast<int8_t>(fetch());
        if (take) {
            cpu_.pc += offset;
        }
    }

    void modify_bit(unsigned bit, bool set) {
        const auto address = direct(fetch());
        const auto value = read_bus(address);
        const auto mask = 1u << bit;
        write(apu_, address, set ? value | mask : value & ~mask);
    }
};

} // namespace

bool spc700_start_driver(SnesApu &apu) {
    if (apu.phase != APU_PROGRAM_LOADED || apu.driver_started) {
        return apu.driver_started;
    }
    apu.driver = {};
    apu.driver.pc = apu.entry_address;
    apu.driver.stack = 0xEF;
    Spc700 startup(apu);
    for (unsigned instruction = 0; instruction < 30000; ++instruction) {
        if (!startup.step()) {
            return false;
        }
        if (apu.control & 7) {
            apu.driver_started = true;
            return true;
        }
    }
    return false;
}

bool spc700_run_until_reply(SnesApu &apu) {
    Spc700 cpu(apu);
    // The driver's copy commands can move almost the entire 64 KiB sound RAM before replying.
    for (unsigned instruction = 0; instruction < 500000 && apu.command_pending; ++instruction) {
        if (!cpu.step()) {
            return false;
        }
    }
    // Let the driver consume the rest of this packet before the main CPU changes the ports.
    return !apu.command_pending && cpu.finish_reply();
}

bool spc700_settle_cleared_ports(SnesApu &apu) {
    Spc700 cpu(apu);
    if (!cpu.finish_reply()) {
        return false;
    }
    for (unsigned poll = 0; poll < 32; ++poll) {
        const auto address = apu.driver.pc;
        if (!cpu.resume_from_port_read() || !cpu.finish_reply()) {
            return false;
        }
        if (apu.driver.pc == address) {
            apu.command_pending = false;
            return true;
        }
    }
    return false;
}
