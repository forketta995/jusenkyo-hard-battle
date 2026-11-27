#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct SnesAudio SnesAudio;

typedef enum {
    APU_WAITING_FOR_UPLOAD,
    APU_RECEIVING_BLOCK,
    APU_PROGRAM_LOADED,
} SnesApuPhase;

typedef struct {
    uint16_t pc;
    uint8_t accumulator;
    uint8_t x;
    uint8_t y;
    uint8_t stack;
    uint8_t status;
} Spc700State;

typedef struct {
    uint8_t ram[0x10000];
    uint8_t from_cpu[4];
    uint8_t to_cpu[4];
    uint8_t next_index;
    uint16_t write_address;
    uint16_t entry_address;
    uint32_t uploaded_bytes;
    SnesApuPhase phase;
    uint8_t dsp[128];
    uint8_t dsp_address;
    uint8_t control;
    uint8_t timer_targets[3];
    Spc700State driver;
    uint8_t driver_opcode;
    bool driver_started;
    bool command_pending;
    bool driver_fault;
    uint32_t acknowledged_packets;
    SnesAudio *audio;
} SnesApu;

#ifdef __cplusplus
void snes_apu_reset(SnesApu &apu);
uint8_t snes_apu_read(SnesApu &apu, unsigned port);
void snes_apu_write(SnesApu &apu, unsigned port, uint8_t value);
void snes_apu_advance(SnesApu &apu, unsigned dots);
bool spc700_start_driver(SnesApu &apu);
bool spc700_run_until_reply(SnesApu &apu);
bool spc700_settle_cleared_ports(SnesApu &apu);
#endif
