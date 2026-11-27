"""Translate statically reachable blocks in a verified cartridge's boot path."""

import argparse
from collections import deque
import hashlib
from pathlib import Path
import re
import tempfile
import tomllib
import zlib


IMPLIED = {
    0x08: "cpu_push_status(cpu)",
    0x0A: "cpu_shift_accumulator_left(cpu)",
    0x0B: "cpu_push_direct_page(cpu)",
    0x18: "cpu_clear_carry(cpu)",
    0x1A: "cpu_increment_accumulator(cpu)",
    0x1B: "cpu_transfer_accumulator_to_stack(cpu)",
    0x28: "cpu_pull_status(cpu)",
    0x2A: "cpu_rotate_accumulator_left(cpu)",
    0x2B: "cpu_pull_direct_page(cpu)",
    0x38: "cpu_set_carry(cpu)",
    0x3A: "cpu_decrement_accumulator(cpu)",
    0x3B: "cpu_transfer_stack_to_accumulator(cpu)",
    0x48: "cpu_push_accumulator(cpu)",
    0x4A: "cpu_shift_accumulator_right(cpu)",
    0x58: "cpu_clear_interrupt_disable(cpu)",
    0x5A: "cpu_push_y(cpu)",
    0x5B: "cpu_transfer_accumulator_to_direct_page(cpu)",
    0x6A: "cpu_rotate_accumulator_right(cpu)",
    0x68: "cpu_pull_accumulator(cpu)",
    0x78: "cpu_set_interrupt_disable(cpu)",
    0x7A: "cpu_pull_y(cpu)",
    0x8B: "cpu_push_data_bank(cpu)",
    0x88: "cpu_decrement_y(cpu)",
    0x8A: "cpu_transfer_x_to_accumulator(cpu)",
    0x98: "cpu_transfer_y_to_accumulator(cpu)",
    0x9A: "cpu_transfer_x_to_stack(cpu)",
    0x9B: "cpu_transfer_x_to_y(cpu)",
    0xA8: "cpu_transfer_accumulator_to_y(cpu)",
    0xAA: "cpu_transfer_accumulator_to_x(cpu)",
    0xAB: "cpu_pull_data_bank(cpu)",
    0xBA: "cpu_transfer_stack_to_x(cpu)",
    0xBB: "cpu_transfer_y_to_x(cpu)",
    0xC8: "cpu_increment_y(cpu)",
    0xCA: "cpu_decrement_x(cpu)",
    0xD8: "cpu_clear_status(cpu, CPU_DECIMAL)",
    0xDA: "cpu_push_x(cpu)",
    0xE8: "cpu_increment_x(cpu)",
    0xEB: "cpu_exchange_accumulator_bytes(cpu)",
    0xFA: "cpu_pull_x(cpu)",
    0xF8: "cpu_set_status(cpu, CPU_DECIMAL)",
    0xFB: "cpu_exchange_carry_emulation(cpu)",
}
OPERAND = {
    0x3D: (2, "cpu_and_accumulator_indexed_x"),
    0xF9: (2, "cpu_subtract_accumulator_indexed_y"),
    0x03: (1, "cpu_or_accumulator_stack_relative"),
    0x05: (1, "cpu_or_accumulator_direct"),
    0x5E: (2, "cpu_shift_indexed_x_right"),
    0xFD: (2, "cpu_subtract_accumulator_indexed_x"),
    0x19: (2, "cpu_or_accumulator_indexed_y"),
    0x1D: (2, "cpu_or_accumulator_indexed_x"),
    0x1E: (2, "cpu_shift_indexed_x_left"),
    0x92: (1, "cpu_store_accumulator_indirect"),
    0xF2: (1, "cpu_subtract_accumulator_indirect"),
    0x0D: (2, "cpu_or_accumulator_absolute"),
    0x11: (1, "cpu_or_accumulator_indirect_indexed_y"),
    0x12: (1, "cpu_or_accumulator_indirect"),
    0x2D: (2, "cpu_and_accumulator_absolute"),
    0x31: (1, "cpu_and_accumulator_indirect_indexed_y"),
    0x3F: (3, "cpu_and_accumulator_long_indexed_x"),
    0x4D: (2, "cpu_xor_accumulator_absolute"),
    0xDD: (2, "cpu_compare_accumulator_indexed_x"),
    0xDF: (3, "cpu_compare_accumulator_long_indexed_x"),
    0xDE: (2, "cpu_decrement_indexed_x"),
    0xCE: (2, "cpu_decrement_absolute"),
    0x06: (1, "cpu_shift_direct_left"),
    0x0C: (2, "cpu_test_and_set_absolute"),
    0x26: (1, "cpu_rotate_direct_left"),
    0x36: (1, "cpu_rotate_direct_indexed_x_left"),
    0x46: (1, "cpu_shift_direct_right"),
    0x54: (2, "cpu_move_block_forward"),
    0x63: (1, "cpu_add_accumulator_stack_relative"),
    0x64: (1, "cpu_store_zero_direct"),
    0x65: (1, "cpu_add_accumulator_direct"),
    0x6D: (2, "cpu_add_accumulator_absolute"),
    0x6F: (3, "cpu_add_accumulator_long"),
    0x71: (1, "cpu_add_accumulator_indirect_indexed_y"),
    0x72: (1, "cpu_add_accumulator_indirect"),
    0x74: (1, "cpu_store_zero_direct_indexed_x"),
    0x75: (1, "cpu_add_accumulator_direct_indexed_x"),
    0x79: (2, "cpu_add_accumulator_indexed_y"),
    0x7D: (2, "cpu_add_accumulator_indexed_x"),
    0x83: (1, "cpu_store_accumulator_stack_relative"),
    0x85: (1, "cpu_store_accumulator_direct"),
    0x86: (1, "cpu_store_x_direct"),
    0x84: (1, "cpu_store_y_direct"),
    0x87: (1, "cpu_store_accumulator_indirect_long"),
    0x8C: (2, "cpu_store_y_absolute"),
    0x8D: (2, "cpu_store_accumulator"),
    0x8E: (2, "cpu_store_x"),
    0x8F: (3, "cpu_store_accumulator_long"),
    0x91: (1, "cpu_store_accumulator_indirect_indexed_y"),
    0x95: (1, "cpu_store_accumulator_direct_indexed_x"),
    0x97: (1, "cpu_store_accumulator_indirect_long_indexed_y"),
    0x99: (2, "cpu_store_accumulator_indexed_y"),
    0x9C: (2, "cpu_store_zero"),
    0x9D: (2, "cpu_store_accumulator_indexed_x"),
    0x9E: (2, "cpu_store_zero_indexed_x"),
    0x9F: (3, "cpu_store_accumulator_long_indexed_x"),
    0xA4: (1, "cpu_load_y_direct"),
    0xA3: (1, "cpu_load_accumulator_stack_relative"),
    0xA5: (1, "cpu_load_accumulator_direct"),
    0xA6: (1, "cpu_load_x_direct"),
    0xA7: (1, "cpu_load_accumulator_indirect_long"),
    0xAD: (2, "cpu_load_accumulator_absolute"),
    0xAC: (2, "cpu_load_y_absolute"),
    0xAE: (2, "cpu_load_x_absolute"),
    0xAF: (3, "cpu_load_accumulator_long"),
    0xB1: (1, "cpu_load_accumulator_indirect_indexed_y"),
    0xB2: (1, "cpu_load_accumulator_indirect"),
    0xB5: (1, "cpu_load_accumulator_direct_indexed_x"),
    0xB7: (1, "cpu_load_accumulator_indirect_long_indexed_y"),
    0xB9: (2, "cpu_load_accumulator_indexed_y"),
    0xBC: (2, "cpu_load_y_indexed_x"),
    0xBD: (2, "cpu_load_accumulator_indexed_x"),
    0xBE: (2, "cpu_load_x_indexed_y"),
    0xBF: (3, "cpu_load_accumulator_long_indexed_x"),
    0xC2: (1, "cpu_clear_status"),
    0xC4: (1, "cpu_compare_y_direct"),
    0xC3: (1, "cpu_compare_accumulator_stack_relative"),
    0xC5: (1, "cpu_compare_accumulator_direct"),
    0xC6: (1, "cpu_decrement_direct"),
    0xCD: (2, "cpu_compare_accumulator_absolute"),
    0xCF: (3, "cpu_compare_accumulator_long"),
    0xD1: (1, "cpu_compare_accumulator_indirect_indexed_y"),
    0xD4: (1, "cpu_push_indirect_word"),
    0xD5: (1, "cpu_compare_accumulator_direct_indexed_x"),
    0xD9: (2, "cpu_compare_accumulator_indexed_y"),
    0xE2: (1, "cpu_set_status"),
    0xE3: (1, "cpu_subtract_accumulator_stack_relative"),
    0xE4: (1, "cpu_compare_x_direct"),
    0xE5: (1, "cpu_subtract_accumulator_direct"),
    0xE6: (1, "cpu_increment_direct"),
    0xED: (2, "cpu_subtract_accumulator_absolute"),
    0xEE: (2, "cpu_increment_absolute"),
    0xF4: (2, "cpu_push_word"),
    0xFE: (2, "cpu_increment_indexed_x"),
}
BRANCHES = {
    0x10: ("CPU_NEGATIVE", False),
    0x30: ("CPU_NEGATIVE", True),
    0x90: ("CPU_CARRY", False),
    0xB0: ("CPU_CARRY", True),
    0xD0: ("CPU_ZERO", False),
    0xF0: ("CPU_ZERO", True),
}


def load_rom(config, path):
    rom = path.read_bytes()
    if hashlib.sha256(rom).hexdigest() != config["sha256"]:
        raise ValueError("ROM SHA-256 does not match the supported revision.")
    if config["mapping"] != "hirom":
        raise ValueError("This recompiler currently requires HiROM.")
    return rom


def reset_address(rom):
    address = int.from_bytes(rom[0xFFFC:0xFFFE], "little")
    if len(rom) < 0x10000 or address < 0x8000:
        raise ValueError("The HiROM reset vector does not point to ROM.")
    return address


def read_program(rom, address, size):
    bank, offset = divmod(address, 0x10000)
    if ((bank & 0x40) == 0 and offset < 0x8000) or bank in (0x7E, 0x7F):
        raise ValueError(f"Program address {address:06X} is outside supported HiROM.")
    if offset + size > 0x10000:
        raise ValueError(f"Instruction at {address:06X} crosses a bank boundary.")
    start = address & 0x3FFFFF
    if start + size > len(rom):
        raise ValueError(f"Program address {address:06X} is outside the ROM image.")
    return int.from_bytes(rom[start:start + size], "little")


def advance(address, size):
    return (address & 0xFF0000) | ((address + size) & 0xFFFF)


def decode_block(rom, entry, state, returns, stops, conditional_stops, indexed_jumps,
                 stack_dispatches, interrupt_returns, preview_overrides):
    address = entry
    emulation, carry, status, saved_status = state
    lines = []
    for _ in range(4096):
        if address in stops:
            lines.append(f"cpu->pc = 0x{address:06X};")
            lines.append("return cpu->unmapped_access ? BOOT_UNMAPPED_ACCESS : BOOT_CHECKPOINT;")
            return lines, []
        if address in conditional_stops:
            checkpoint = conditional_stops[address]
            if address != entry:
                lines.append(f"cpu->pc = 0x{address:06X};")
                return lines, [(address, (emulation, carry, status, saved_status), returns)]
            lines.extend([
                f"if (!cpu->running_frames && "
                f"cpu->work_ram[0x{checkpoint['wram_offset']:05X}] == 0x{checkpoint['value']:02X}) {{",
                "    return cpu->unmapped_access ? BOOT_UNMAPPED_ACCESS : BOOT_CHECKPOINT;",
                "}",
            ])
        if address in preview_overrides:
            override = preview_overrides[address]
            audio_check = "!cpu->apu.audio && " if override.get("silent_only") else ""
            lines.extend([
                f"if (cpu->apply_preview_patches && {audio_check}"
                f"cpu->work_ram[0x{override['wram_offset']:05X}] == 0x{override['wram_value']:02X}) {{",
                f"    cpu->accumulator = 0x{override['value']:04X};",
                "}",
            ])
        opcode = read_program(rom, address, 1)
        operand_address = advance(address, 1)
        size = 0
        if opcode == 0xEA:
            address = advance(address, 1)
            continue
        if opcode == 0x4B:
            statement = f"cpu_push_program_bank(cpu, 0x{address >> 16:02X})"
        elif opcode in IMPLIED:
            statement = IMPLIED[opcode]
            if opcode == 0x08:
                saved_status = (*saved_status, (carry, status))
            elif opcode == 0x28:
                if not saved_status:
                    raise ValueError(f"Status pull at {address:06X} has no known status push.")
                carry, status = saved_status[-1]
                saved_status = saved_status[:-1]
                if emulation:
                    status |= 0x30
            elif opcode == 0x18:
                carry = False
            elif opcode == 0x38:
                carry = True
            elif opcode == 0xD8:
                status &= ~0x08
            elif opcode == 0xF8:
                status |= 0x08
            elif opcode in (0x0A, 0x2A, 0x4A, 0x6A):
                carry = None
            elif opcode == 0xFB:
                if carry is None:
                    raise ValueError(f"Mode after XCE at {address:06X} is unknown.")
                carry, emulation = emulation, carry
                if emulation:
                    status |= 0x30
        elif opcode in OPERAND:
            size, function = OPERAND[opcode]
            operand = read_program(rom, operand_address, size)
            if opcode == 0x54 and status & 0x10:
                raise ValueError(f"Block move at {address:06X} requires 16-bit indexes.")
            statement = f"{function}(cpu, 0x{operand:0{size * 2}X})"
            if opcode in (0xC2, 0xE2):
                status = (status & ~operand) if opcode == 0xC2 else (status | operand)
                if operand & 1:
                    carry = opcode == 0xE2
                if emulation:
                    status |= 0x30
            elif opcode in (
                0x06, 0x1E, 0x26, 0x36, 0x46, 0x5E, 0x63, 0x65, 0x6D, 0x6F, 0x71, 0x72, 0x75, 0x79, 0x7D,
                0xC3, 0xC4, 0xC5, 0xCD, 0xCF, 0xD1, 0xD5, 0xD9, 0xDD, 0xDF,
                0xE3, 0xE4, 0xE5, 0xED, 0xF2, 0xF9, 0xFD
            ):
                carry = None
        elif opcode in (0x09, 0x29, 0x49, 0x69, 0x89, 0xA0, 0xA2, 0xA9, 0xC0, 0xC9, 0xE0, 0xE9):
            width_flag = 0x10 if opcode in (0xA0, 0xA2, 0xC0, 0xE0) else 0x20
            size = 1 if status & width_flag else 2
            operand = read_program(rom, operand_address, size)
            function = {
                0x09: "cpu_or_accumulator",
                0x29: "cpu_and_accumulator",
                0x49: "cpu_xor_accumulator",
                0x69: "cpu_add_accumulator",
                0x89: "cpu_test_accumulator",
                0xA0: "cpu_load_y",
                0xA2: "cpu_load_x",
                0xA9: "cpu_load_accumulator",
                0xC0: "cpu_compare_y",
                0xC9: "cpu_compare_accumulator",
                0xE0: "cpu_compare_x",
                0xE9: "cpu_subtract_accumulator",
            }[opcode]
            statement = f"{function}(cpu, 0x{operand:0{size * 2}X})"
            if opcode in (0x69, 0xC0, 0xC9, 0xE0, 0xE9):
                carry = None
        elif opcode in (0x20, 0x22, 0x40, 0x4C, 0x5C, 0x60, 0x6B, 0x7C, 0x80, 0xFC) or opcode in BRANCHES:
            state = (emulation, carry, status, saved_status)
            if opcode == 0x40:
                resumed = interrupt_returns.get(address, [])
                lines.append("cpu_return_interrupt(cpu);")
                return lines, [
                    (item["target"], (False, None, item["status"], ()), ())
                    for item in resumed
                ]
            if opcode in (0x20, 0x22):
                size = 2 if opcode == 0x20 else 3
                target = read_program(rom, operand_address, size)
                return_address = advance(address, size)
                if opcode == 0x20:
                    target |= address & 0xFF0000
                    lines.append(f"cpu_call_subroutine(cpu, 0x{target:06X}, 0x{return_address & 0xFFFF:04X});")
                else:
                    lines.append(f"cpu_call_long_subroutine(cpu, 0x{target:06X}, 0x{return_address:06X});")
                return lines, [(target, state, (*returns, advance(address, size + 1)))]
            if opcode in (0x4C, 0x5C):
                size = 2 if opcode == 0x4C else 3
                target = read_program(rom, operand_address, size)
                if size == 2:
                    target |= address & 0xFF0000
                lines.append(f"cpu->pc = 0x{target:06X};")
                return lines, [(target, state, returns)]
            if opcode in (0x60, 0x6B):
                if address in stack_dispatches:
                    dispatch = stack_dispatches[address]
                    if opcode == 0x6B:
                        lines.append("cpu_return_long_subroutine(cpu);")
                    else:
                        lines.append(f"cpu_return_subroutine(cpu, 0x{address & 0xFF0000:06X});")
                    direct_return = dispatch.get("allow_direct_return", False)
                    if not direct_return:
                        unsupported = " && ".join(
                            f"cpu->pc != 0x{target:06X}" for target in dispatch["targets"]
                        )
                        lines.append(f"if ({unsupported}) {{")
                        lines.append("    return BOOT_UNKNOWN_BLOCK;")
                        lines.append("}")
                    direct_successors = [(returns[-1], state, returns[:-1])] if direct_return and returns else []
                    continuation = dispatch.get("return_address")
                    if dispatch.get("non_recursive") and continuation in returns:
                        return lines, direct_successors
                    caller_stack = () if dispatch.get("reset_returns") else returns
                    child_returns = (*caller_stack, continuation) if continuation is not None else caller_stack
                    return lines, direct_successors + [
                        (target, state, child_returns)
                        for target in dispatch["targets"]
                    ]
                if not returns:
                    raise ValueError(f"Return at {address:06X} has no statically known caller.")
                if opcode == 0x60:
                    lines.append(f"cpu_return_subroutine(cpu, 0x{address & 0xFF0000:06X});")
                else:
                    lines.append("cpu_return_long_subroutine(cpu);")
                return lines, [(returns[-1], state, returns[:-1])]
            if opcode in (0x7C, 0xFC):
                if address not in indexed_jumps:
                    raise ValueError(f"Indexed jump at {address:06X} has no configured targets.")
                jump = indexed_jumps[address]
                bank = address & 0xFF0000
                table = bank | read_program(rom, operand_address, 2)
                if not jump["indices"]:
                    raise ValueError(f"Indexed jump at {address:06X} has no configured indices.")
                unsupported = " && ".join(f"cpu->x != 0x{index:04X}" for index in jump["indices"])
                lines.append(f"if ({unsupported}) {{")
                lines.append(f"    cpu->pc = 0x{address:06X};")
                lines.append("    return BOOT_UNKNOWN_BLOCK;")
                lines.append("}")
                if opcode == 0xFC:
                    lines.append(f"cpu_call_indexed(cpu, 0x{table:06X}, 0x{advance(address, 2) & 0xFFFF:04X});")
                    child_stacks = [(*returns, advance(address, 3))]
                else:
                    lines.append(f"cpu_jump_indexed(cpu, 0x{table:06X});")
                    continuation = jump.get("return_address")
                    continuations = jump.get("return_addresses", [] if continuation is None else [continuation])
                    child_stacks = [(*returns, value) for value in continuations] or [returns]
                return lines, [
                    (bank | read_program(rom, advance(table, index), 2), state, child_returns)
                    for index in jump["indices"]
                    for child_returns in child_stacks
                ]
            fallthrough = advance(address, 2)
            displacement = read_program(rom, operand_address, 1)
            if displacement >= 128:
                displacement -= 256
            target = advance(fallthrough, displacement)
            if opcode == 0x80:
                lines.append(f"cpu->pc = 0x{target:06X};")
                return lines, [(target, state, returns)]
            flag, branch_if_set = BRANCHES[opcode]
            when_set, when_clear = (target, fallthrough) if branch_if_set else (fallthrough, target)
            lines.append(
                f"cpu->pc = (cpu->status & {flag}) ? "
                f"0x{when_set:06X} : 0x{when_clear:06X};"
            )
            return lines, [(fallthrough, state, returns), (target, state, returns)]
        else:
            raise ValueError(f"Unsupported opcode {opcode:02X} at {address:06X}.")
        lines.append(f"{statement};")
        address = advance(address, 1 + size)
    raise ValueError(f"Block at {entry:06X} exceeded the instruction limit.")


def translate(rom, stops, conditional_stops, indexed_jumps, stack_dispatches,
              interrupt_entries, interrupt_returns, preview_overrides, symbol_prefix):
    entry = reset_address(rom)
    pending = deque([(entry, (True, None, 0x34, ()), ())])
    for interrupt in interrupt_entries:
        pending.append((interrupt["address"], (False, None, 0x04, ()), ()))
    visited = set()
    blocks = {}
    while pending:
        item = pending.popleft()
        if item in visited:
            continue
        if len(visited) >= 750000:
            raise ValueError(f"Boot analysis exceeded the state limit near {item[0]:06X} with {len(item[2])} pending returns.")
        visited.add(item)
        address, state, returns = item
        try:
            lines, successors = decode_block(
                rom, address, state, returns, stops, conditional_stops, indexed_jumps,
                stack_dispatches, interrupt_returns, preview_overrides
            )
        except ValueError as error:
            continuation = f"{returns[-1]:06X}" if returns else "none"
            raise ValueError(f"{error} Block entry {address:06X}, continuation {continuation}.") from error
        if blocks.setdefault(address, lines) != lines:
            raise ValueError(f"Incompatible instruction widths at {address:06X}.")
        pending.extend(successors)

    source = [
        '/* Generated by recompile_65816.py. */',
        '#include "cpu_65816.h"',
        '#include "snes_program.h"',
        '',
        f'const uint32_t {symbol_prefix}_rom_size = {len(rom)};',
        f'const uint32_t {symbol_prefix}_rom_crc = 0x{zlib.crc32(rom):08X};',
        '',
        'static BootResult run_blocks(Cpu65816 *cpu) {',
        '    const uint32_t started_at_frame = cpu->frame_count;',
        '    // Startup can spend several video frames polling clocked peripherals.',
        '    const unsigned step_limit = cpu->running_frames ? 500000 : 2000000;',
        '    for (unsigned steps = 0; steps < step_limit; ++steps) {',
        '        cpu_deliver_irq(cpu);',
        '        const uint32_t block = cpu->pc;',
        '        switch (block) {',
    ]
    for address, lines in sorted(blocks.items()):
        source.append(f'        case 0x{address:06X}:')
        source.extend(f'            {line}' for line in lines)
        if not lines[-1].startswith("return "):
            # Approximate scanline progression until per-instruction timing is available.
            source.append(f'            cpu_advance_raster(cpu, {max(4, len(lines) * 6)});')
            source.append('            break;')
    source.extend([
        '        default: return BOOT_UNKNOWN_BLOCK;',
        '        }',
        '        cpu->last_block = block;',
        '        if (cpu->unmapped_access) {',
        '            return BOOT_UNMAPPED_ACCESS;',
        '        }',
        '        if (cpu->apu.driver_fault) {',
        '            return BOOT_AUDIO_FAULT;',
        '        }',
        '        if (cpu->running_frames && cpu->frame_count != started_at_frame) {',
        '            return BOOT_FRAME;',
        '        }',
        '    }',
        '    return BOOT_STEP_LIMIT;',
        '}',
        '',
        f'BootResult {symbol_prefix}_boot(Cpu65816 *cpu) {{',
        f'    cpu->pc = 0x{entry:06X};',
        '    cpu->running_frames = false;',
        '    return run_blocks(cpu);',
        '}',
        '',
        f'BootResult {symbol_prefix}_continue(Cpu65816 *cpu) {{',
        '    cpu->running_frames = true;',
        '    return run_blocks(cpu);',
        '}',
        '',
    ])
    return '\n'.join(source)


def write_generated(path, source):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", encoding="ascii", newline="\n", dir=path.parent, delete=False
    ) as output:
        temporary = Path(output.name)
        try:
            output.write(source)
            output.close()
            temporary.replace(path)
        finally:
            temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    try:
        config = tomllib.loads(args.config.read_text(encoding="utf-8"))
        symbol_prefix = config["symbol_prefix"]
        if not isinstance(symbol_prefix, str) or not re.fullmatch(r"[a-z][a-z0-9_]*", symbol_prefix):
            raise ValueError("symbol_prefix must start with a lowercase ASCII letter and contain only lowercase letters, digits, and underscores.")
        generated_root = args.build_dir / "src" / "generated"
        output = args.output.resolve()
        if not output.is_relative_to(generated_root.resolve()) or output.suffix != ".c":
            raise ValueError("Output must be a C file under the build directory's src/generated folder.")
        if output == args.rom.resolve():
            raise ValueError("The output path must differ from the ROM path.")
        rom = load_rom(config, args.rom)
        jumps = {jump["address"]: jump for jump in config.get("indexed_jumps", [])}
        dispatches = {item["address"]: item for item in config.get("stack_dispatches", [])}
        conditional_stops = {}
        for checkpoint in config.get("conditional_checkpoints", []):
            if not 0 <= checkpoint["wram_offset"] < 0x20000 or not 0 <= checkpoint["value"] <= 0xFF:
                raise ValueError("Conditional checkpoint must test a byte within work RAM.")
            conditional_stops[checkpoint["address"]] = checkpoint
        interrupt_returns = {}
        for item in config.get("interrupt_returns", []):
            interrupt_returns.setdefault(item["address"], []).append(item)
        preview_overrides = {}
        for override in config.get("preview_accumulator_overrides", []):
            if not (0 <= override["wram_offset"] < 0x20000 and
                    0 <= override["wram_value"] <= 0xFF and 0 <= override["value"] <= 0xFFFF):
                raise ValueError("Preview accumulator override has an invalid WRAM test or accumulator value.")
            preview_overrides[override["address"]] = override
        source = translate(
            rom, set(config["boot_checkpoints"]), conditional_stops, jumps, dispatches,
            config.get("interrupt_entries", []), interrupt_returns, preview_overrides, symbol_prefix,
        )
        write_generated(output, source)
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
