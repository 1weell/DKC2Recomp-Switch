"""Observe SPC commands on the generated C path; interpreter hooks cover LLE."""
import argparse
from pathlib import Path

def apply(directory):
    anchor = '  RecompReturn _pending_skip = RECOMP_RETURN_NORMAL;'
    symbol = 'RecompReturn bank_B5_81FB_M0X0(CpuState *cpu) {'
    matches = [p for p in directory.glob('*.c') if symbol in p.read_text()]
    if len(matches) != 1: raise ValueError('expected one US v1.0 SPC command entry')
    p = matches[0]
    s = p.read_text()
    if s.count(anchor) != 1 or 'cpu_trace_func_entry(cpu, 0xB581FB,' not in s:
        raise ValueError('SPC command trace anchor changed')
    hook = '  Dkc2MusicCommand(cpu->ram[0x1C], cpu->X);'
    if hook not in s:
        s = s.replace(anchor, hook + '\n' + anchor)
        s = s.replace('#include "funcs.h"', '#include "funcs.h"\n#include "dkc2_music.h"')
        p.write_text(s)
    return p

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--generated-dir', type=Path, required=True)
    print(apply(p.parse_args().generated_dir))
