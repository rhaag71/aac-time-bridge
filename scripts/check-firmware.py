#!/usr/bin/env python3
"""After both builds, verify production/bench command and function separation."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--nm', default=str(Path.home() / '.platformio/packages/toolchain-xtensa-esp32/bin/xtensa-esp32-elf-nm'))
args = parser.parse_args()
root = Path(__file__).resolve().parent.parent
for env in ('nodemcu-32s', 'watchdog-bench'):
    directory = root / '.pio/build' / env
    binary = (directory / 'firmware.bin').read_bytes()
    symbols = subprocess.check_output([args.nm, '-C', str(directory / 'firmware.elf')], text=True)
    bench = env == 'watchdog-bench'
    assert (b'wedge watchdog' in binary) == bench, env
    assert ('Watchdog::wedgeForBench()' in symbols) == bench, env
    assert b'WEDGE WATCHDOG' not in binary, env
    assert b'FACTORY RESET' not in binary, env
    assert b'factory reset' in binary, env
    assert b"name='password' type='text'" in binary, env
    assert b"name='password' type='password'" not in binary, env
    assert b"AAC / TIME BRIDGE" in binary and b"Every second needs a source." in binary, env
    assert b"--:--:--" in binary and b"UNSYNCHRONIZED" in binary, env
    assert b"/ui.css" in binary and b"/ui.js" in binary, env
    assert b"method='post' action='/manage/reboot'" in binary, env
    assert b"method='post' action='/manage/factory-reset'" in binary, env
    assert b"new Date(" not in binary and b"Intl.DateTimeFormat" not in binary, env
    assert b"login" not in binary.lower() and b"api key" not in binary.lower(), env
    print(f'{env}: lowercase commands, visible new-password field, and bench isolation passed')
