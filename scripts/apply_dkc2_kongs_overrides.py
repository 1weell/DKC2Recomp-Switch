#!/usr/bin/env python3
"""Route optional Kong callbacks through their native instruction hooks.

Generated direct C calls bypass interpreter pre-opcode hooks. Keep original
characters on their compiled path; selected replacements use the existing
paired interpreter ABI for these short RTS routines only.
"""
import argparse
from pathlib import Path

CALLBACKS = {
    'CODE_B9D8AA_M0X0': 0xB9D8AC,
    'CODE_B9DCE8_M0X0': 0xB9DCEA,
    'CODE_B9D8BC_M0X0': 0xB9D8BE,
    'CODE_B9D965_M0X0': 0xB9D967,
    'CODE_B9DFD3_M0X0': 0xB9DFD5,
    'update_held_sprite_position_M0X0': 0xB39FE7,
    'glide_action_M0X0': 0xB8C921,
}


def adapt(text, symbol, pc):
    entry = f'RecompReturn {symbol}(CpuState *cpu) {{'
    if text.count(entry) != 1:
        raise ValueError(f'expected one definition of {symbol}')
    # Verify the PC from the generated trace, not the older symbolic name.
    if f'cpu_trace_func_entry(cpu, 0x{pc:06X}, "{symbol}")' not in text:
        raise ValueError(f'generated entry address changed for {symbol}')
    wrapper = (entry + '\n  if (Dkc2KongsUseCallbacks(cpu->ram))\n'
               f'    return interp_tier_run_call_frame(cpu, 0x{pc:06x}u, '
               f'0x{pc:06x}u, 2, NULL);\n')
    if wrapper not in text:
        text = text.replace(entry, wrapper, 1)
    include = '#include "dkc2_kongs.h"'
    if include not in text:
        marker = '#include "funcs.h"'
        if text.count(marker) != 1:
            raise ValueError('missing unique generated include anchor')
        text = text.replace(marker, marker + '\n' + include, 1)
    return text


def apply(directory):
    units = {p: p.read_text() for p in directory.glob('*.c')}
    changed = []
    for symbol, pc in CALLBACKS.items():
        matches = [p for p, text in units.items()
                   if f'RecompReturn {symbol}(CpuState *cpu) {{' in text]
        if len(matches) != 1:
            raise ValueError(f'expected one generated unit for {symbol}, found {len(matches)}')
        p = matches[0]
        updated = adapt(units[p], symbol, pc)
        if updated != units[p]:
            p.write_text(updated)
            units[p] = updated
        changed.append(p)
    return changed


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated-dir', type=Path, required=True)
    args = parser.parse_args()
    for path in apply(args.generated_dir):
        print(path)
