#include "snes_apu.h"
#include "snes_audio.h"

namespace {

bool ports_cleared(const SnesApu &apu) {
    return apu.from_cpu[0] == 0 && apu.from_cpu[1] == 0 &&
           apu.from_cpu[2] == 0 && apu.from_cpu[3] == 0;
}

void begin_command(SnesApu &apu, uint8_t token) {
    const uint16_t address = apu.from_cpu[2] | (apu.from_cpu[3] << 8);
    apu.to_cpu[0] = token;
    if (apu.from_cpu[1] == 0) {
        apu.entry_address = address;
        apu.phase = APU_PROGRAM_LOADED;
        if (apu.audio) {
            apu.driver_fault = !apu.audio->start(apu);
        }
    } else {
        apu.write_address = address;
        apu.next_index = 0;
        apu.phase = APU_RECEIVING_BLOCK;
    }
}

} // namespace

void snes_apu_reset(SnesApu &apu) {
    apu = {};
    apu.to_cpu[0] = 0xAA;
    apu.to_cpu[1] = 0xBB;
    apu.dsp[0x6C] = 0xE0;
}

uint8_t snes_apu_read(SnesApu &apu, unsigned port) {
    if (apu.audio && apu.phase == APU_PROGRAM_LOADED && !apu.driver_fault) {
        return apu.audio->read(port);
    }
    if (apu.command_pending && !apu.driver_fault) {
        const auto requested_token = apu.from_cpu[0];
        const auto packets = apu.acknowledged_packets;
        if (apu.driver_started && ports_cleared(apu)) {
            apu.driver_fault = !spc700_settle_cleared_ports(apu);
        } else {
            apu.driver_fault = !spc700_start_driver(apu) || !spc700_run_until_reply(apu);
        }
        if (!apu.driver_fault && port == 0 && apu.acknowledged_packets != packets) {
            // The polling read observes the driver's echo before its next port write.
            return requested_token;
        }
    }
    return apu.to_cpu[port];
}

void snes_apu_write(SnesApu &apu, unsigned port, uint8_t value) {
    if (apu.audio && apu.phase == APU_PROGRAM_LOADED && !apu.driver_fault) {
        apu.audio->write(port, value);
        return;
    }
    if (apu.driver_started && apu.command_pending && !apu.driver_fault && value != 0 &&
        ports_cleared(apu)) {
        // The main CPU can release the transfer by clearing the ports without polling again.
        apu.driver_fault = !spc700_settle_cleared_ports(apu);
    }
    apu.from_cpu[port] = value;
    if (port != 0) {
        return;
    }
    if (apu.phase == APU_PROGRAM_LOADED) {
        apu.command_pending = apu.driver_started || value != 0;
        return;
    }
    if (apu.phase == APU_WAITING_FOR_UPLOAD && value == 0xCC) {
        begin_command(apu, value);
    } else if (apu.phase == APU_RECEIVING_BLOCK) {
        const uint8_t delta = value - apu.next_index;
        if (delta == 0) {
            apu.ram[apu.write_address++] = apu.from_cpu[1];
            ++apu.next_index;
            ++apu.uploaded_bytes;
            apu.to_cpu[0] = value;
        } else if (delta < 0x80) {
            begin_command(apu, value);
        }
    }
}

void snes_apu_advance(SnesApu &apu, unsigned dots) {
    if (apu.audio) {
        apu.audio->advance(dots);
    }
}
