#!/usr/bin/env python3
# Keeps AyuGram's prepare.py working with the current MSYS2: the MINGW64
# packages it asks for are gone, upstream Telegram Desktop moved to UCRT64.
import sys
from pathlib import Path

p = Path(sys.argv[1]) / 'Telegram' / 'build' / 'prepare' / 'prepare.py'
s = p.read_text(encoding='utf-8')
orig = s
s = s.replace(r"msys64\\mingw64\\bin", r"msys64\\ucrt64\\bin")
s = s.replace('mingw-w64-x86_64-', 'mingw-w64-ucrt-x86_64-')
s = s.replace('releases/download/2025-08-30/msys2-base-x86_64-20250830.sfx.exe',
              'releases/download/2026-09-27/msys2-base-x86_64-20260927.sfx.exe')
if s == orig:
    print('prepare.py: nothing to fix')
else:
    p.write_text(s, encoding='utf-8', newline='')
    print('prepare.py: switched MSYS2 packages to UCRT64')
