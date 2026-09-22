"""Portable reporter checks; libnx lock shim tests wiring, not platform threading."""
from pathlib import Path
import json
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class SwitchReporter(unittest.TestCase):
    def test_paths_rotation_limit_fatal_and_lock_wiring(self):
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            (temp / "switch.h").write_text("""
#ifndef SWITCH_TEST_SHIM
#define SWITCH_TEST_SHIM
typedef struct { unsigned unused; } RMutex;
extern unsigned test_depth, test_locks;
static inline void rmutexLock(RMutex *m) { (void)m; ++test_depth; ++test_locks; }
static inline void rmutexUnlock(RMutex *m) { (void)m; --test_depth; }
#endif
""")
            (temp / "main.c").write_text(r"""
#include "host_report.h"
#include <stdio.h>
unsigned test_depth, test_locks;
void RtlApuLock(void); void RtlApuUnlock(void);
int main(int argc, char **argv) {
  if (argc != 2) return 1;
  RtlApuLock(); RtlApuLock(); RtlApuUnlock(); RtlApuUnlock();
  if (test_depth || test_locks != 2) return 2;
  host_report_set_output_directory(argv[1]);
  host_report_init("test", "synthetic");
  if (host_report_has_fatal()) return 3;
  for (unsigned i=0; i<5000; ++i) host_report_breadcrumb("entry %u", i);
  host_report_fatal("synthetic fatal");
  if (!host_report_has_fatal()) return 4;
  fputs("{", stdout); host_report_dump_json(stdout); fputs("\"end\":true}", stdout);
  return 0;
}
""")
            executable = temp / "reporter.exe"
            subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                            "-I" + str(temp), "-I" + str(ROOT / "snesrecomp/runner/src"),
                            str(ROOT / "runner/switch_host.c"), str(temp / "main.c"),
                            "-o", str(executable)], check=True, capture_output=True)
            logs = temp / "logs"
            logs.mkdir()
            def run():
                result = subprocess.run([str(executable), str(logs)], cwd=temp,
                                        check=True, text=True, capture_output=True)
                self.assertTrue(json.loads(result.stdout)["switch"]["fatal"])
            run()
            first = (logs / "boot.log").read_bytes()
            self.assertLessEqual(len(first.splitlines()), 4097)
            self.assertTrue(first.endswith(b"[fatal] synthetic fatal\n"))
            self.assertFalse((temp / "boot.log").exists())
            run()
            self.assertEqual((logs / "boot.previous.log").read_bytes(), first)
            (logs / "boot.previous.log").unlink()
            (logs / "boot.previous.log").mkdir()
            (logs / "boot.previous.log/block").write_text("blocked rotation")
            run()
            self.assertEqual((logs / "boot.log").read_bytes(), first)

if __name__ == "__main__": unittest.main()
