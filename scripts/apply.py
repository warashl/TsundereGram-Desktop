#!/usr/bin/env python3
# TsundereGram Desktop: applies branding and TsundereGram features
# on top of the AyuGram Desktop sources (GPLv3).
import argparse, base64, io, json, os, re, shutil, subprocess, sys, uuid
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
ASSETS = REPO / 'assets'
STATE = 'tsundere_state.json'

APP_ID = '{8E1C5B7A-3D24-4F6B-9A0E-5C7D2B1F4E93}'
# All upstream files get this mtime, so a restored build tree stays valid
# and only files whose content really changed are rebuilt.
FIXED_TIME = 1577836800  # 2020-01-01
MANIFEST = 'out/.tsg_manifest.json'

def log(*a):
    print('[tsundere]', *a, flush=True)

class Patcher:
    def __init__(self, root):
        self.root = Path(root)
        self.tg = self.root / 'Telegram'
        self.touched = set()
        self.added = set()

    def path(self, rel):
        return self.root / rel

    def read(self, rel):
        return self.path(rel).read_text(encoding='utf-8')

    def write(self, rel, text):
        self.path(rel).write_text(text, encoding='utf-8', newline='')
        self.touched.add(rel)

    def replace(self, rel, old, new, count=1, optional=False):
        text = self.read(rel)
        if new in text and old not in text:
            log('already applied in', rel)
            return
        n = text.count(old)
        if n == 0:
            if optional:
                log('WARN: pattern not found in', rel, repr(old[:60]))
                return
            raise SystemExit('pattern not found in %s: %r' % (rel, old[:120]))
        if count and n < count:
            raise SystemExit('pattern found %d < %d times in %s' % (n, count, rel))
        text = text.replace(old, new) if count == 0 else text.replace(old, new, count)
        self.write(rel, text)
        log('patched', rel)

    def regex(self, rel, pattern, repl, flags=0, expect=True):
        text = self.read(rel)
        new, n = re.subn(pattern, repl, text, flags=flags)
        if n == 0 and expect:
            raise SystemExit('regex not found in %s: %r' % (rel, pattern))
        if new != text:
            self.write(rel, new)
            log('patched(re)', rel, n)

    def put_bytes(self, rel, data):
        p = self.path(rel)
        existed = p.exists()
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
        (self.touched if existed else self.added).add(rel)

    def put_text(self, rel, text):
        self.put_bytes(rel, text.encode('utf-8'))

# ---------------------------------------------------------------- icons

def load_master():
    from PIL import Image
    parts = sorted(ASSETS.glob('icon_square_1024.webp.b64.*'), key=lambda f: int(f.suffix[1:]))
    data = base64.b64decode(''.join(f.read_text().strip() for f in parts))
    return Image.open(io.BytesIO(data)).convert('RGBA')

def masked(img, kind, size, margin=0.0):
    from PIL import Image, ImageDraw
    big = 4 * size
    inner = int(round(big * (1 - 2 * margin)))
    off = (big - inner) // 2
    mask = Image.new('L', (big, big), 0)
    d = ImageDraw.Draw(mask)
    box = (off, off, off + inner - 1, off + inner - 1)
    if kind == 'circle':
        d.ellipse(box, fill=255)
    elif kind == 'rounded':
        d.rounded_rectangle(box, radius=int(inner * 0.225), fill=255)
    else:
        d.rectangle(box, fill=255)
    layer = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    layer.paste(img.resize((inner, inner), Image.LANCZOS), (off, off))
    out = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    out.paste(layer, (0, 0), mask)
    return out.resize((size, size), Image.LANCZOS)

def png_bytes(img):
    b = io.BytesIO()
    img.save(b, 'PNG', optimize=True)
    return b.getvalue()

def ico_bytes(img):
    b = io.BytesIO()
    big = masked(img, 'rounded', 256)
    big.save(b, 'ICO', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    return b.getvalue()

def apply_icons(p):
    master = load_master()
    art = 'Telegram/Resources/art/'
    for s in (16, 32, 48, 64, 128, 256, 512):
        p.put_bytes(art + 'icon%d.png' % s, png_bytes(masked(master, 'rounded', s)))
        p.put_bytes(art + 'icon%d@2x.png' % s, png_bytes(masked(master, 'rounded', s * 2)))
    p.put_bytes(art + 'icon_round512@2x.png', png_bytes(masked(master, 'circle', 1024)))
    p.put_bytes(art + 'logo_256.png', png_bytes(masked(master, 'rounded', 256, margin=0.06)))
    p.put_bytes(art + 'logo_256_no_margin.png', png_bytes(masked(master, 'rounded', 256)))
    ico = ico_bytes(master)
    p.put_bytes(art + 'icon256.ico', ico)
    p.put_bytes(art + 'ayu/default/app_icon.ico', ico)
    app_png = png_bytes(masked(master, 'rounded', 512))
    p.put_bytes(art + 'ayu/default/app.png', app_png)
    circle_png = png_bytes(masked(master, 'circle', 512))
    svg = ('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
           'width="512" height="512" viewBox="0 0 512 512"><image width="512" height="512" '
           'xlink:href="data:image/png;base64,%s"/></svg>' % base64.b64encode(circle_png).decode())
    p.put_text(art + 'ayu/default/app.svg', svg)
    qrc = 'Telegram/Resources/qrc/ayu/ayu.qrc'
    p.replace(qrc,
        '<file alias="art/ayu/default/app.svg">',
        '<file alias="art/ayu/default/app.png">../../art/ayu/default/app.png</file>\n'
        '        <file alias="art/ayu/default/app.svg">')

# ---------------------------------------------------------------- branding

def apply_branding(p):
    v = 'Telegram/SourceFiles/core/version.h'
    p.regex(v, r'constexpr auto AppId = "\{[0-9A-F-]+\}"_cs;',
            'constexpr auto AppId = "%s"_cs;' % APP_ID)
    p.replace(v, 'constexpr auto AppNameOld = "AyuGram for Windows"_cs;',
              'constexpr auto AppNameOld = "TsundereGram for Windows"_cs;')
    p.replace(v, 'constexpr auto AppName = "AyuGram Desktop"_cs;',
              'constexpr auto AppName = "TsundereGram Desktop"_cs;')
    p.replace(v, 'constexpr auto AppFile = "AyuGram"_cs;',
              'constexpr auto AppFile = "TsundereGram"_cs;')

    c = 'Telegram/CMakeLists.txt'
    p.replace(c, 'set(output_name "AyuGram")', 'set(output_name "TsundereGram")')
    p.replace(c, 'OR NOT "${output_name}" STREQUAL "AyuGram")',
              'OR NOT "${output_name}" STREQUAL "TsundereGram")')

    rc = 'Telegram/Resources/winrc/Telegram.rc'
    p.replace(rc, 'VALUE "CompanyName", "Radolyn Labs"', 'VALUE "CompanyName", "TsundereGram"')
    p.replace(rc, 'VALUE "FileDescription", "AyuGram Desktop"', 'VALUE "FileDescription", "TsundereGram Desktop"')
    p.replace(rc, 'VALUE "ProductName", "AyuGram Desktop"', 'VALUE "ProductName", "TsundereGram Desktop"')

    # English base strings: rename the client in values only (keys stay).
    ls = 'Telegram/Resources/langs/lang.strings'
    text = p.read(ls)
    out = []
    for line in text.split('\n'):
        m = re.match(r'^("[^"]+"\s*=\s*")(.*)$', line)
        if m and 'AyuGram' in m.group(2):
            line = m.group(1) + m.group(2).replace('AyuGram', 'TsundereGram')
        out.append(line)
    text = '\n'.join(out)
    text = re.sub(r'^"ayu_SettingsWatermark" = ".*";$',
                  '"ayu_SettingsWatermark" = "TsundereGram Desktop is based on AyuGram by Radolyn Labs.";',
                  text, flags=re.M)
    p.write(ls, text)

    # Translations downloaded at runtime: rename the client as well.
    al = 'Telegram/SourceFiles/ayu/ayu_lang.cpp'
    p.replace(al,
        '		Lang::GetInstance().resetValue(key.toUtf8());\n'
        '		Lang::GetInstance().applyValue(key.toUtf8(), val.toUtf8());',
        '		val = val.replace(qsl("AyuGram"), qsl("TsundereGram"));\n'
        '		if (key == qsl("ayu_SettingsWatermark")) {\n'
        '			continue;\n'
        '		}\n'
        '		Lang::GetInstance().resetValue(key.toUtf8());\n'
        '		Lang::GetInstance().applyValue(key.toUtf8(), val.toUtf8());')
    apply_icons(p)

# ---------------------------------------------------------------- features

def apply_features(p):
    feat = REPO / 'scripts' / 'features.py'
    if not feat.exists():
        log('no features yet')
        return
    sys.path.insert(0, str(REPO / 'scripts'))
    import features
    features.apply(p, REPO, log)

def git_top(path):
    return subprocess.run(['git', '-C', str(path), 'rev-parse', '--show-toplevel'],
                          capture_output=True, text=True, check=True).stdout.strip()

def revert(root, files_touched, files_added):
    root = Path(root)
    for rel in files_added:
        f = root / rel
        if f.exists():
            f.unlink()
            log('removed', rel)
        d = f.parent
        while d != root and d.exists() and not any(d.iterdir()):
            d.rmdir()
            d = d.parent
    for rel in files_touched:
        f = root / rel
        top = git_top(f.parent)
        subprocess.run(['git', '-C', top, 'checkout', '--', str(f)], check=True)
        log('reverted', rel)

import hashlib

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def freeze(root):
    n = 0
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in ('.git', 'out')]
        for f in filenames:
            try:
                os.utime(os.path.join(dirpath, f), (FIXED_TIME, FIXED_TIME))
                n += 1
            except OSError:
                pass
    log('froze mtime of', n, 'files')

def stabilize(root, files):
    mf = Path(root) / MANIFEST
    manifest = json.loads(mf.read_text()) if mf.exists() else {}
    kept = 0
    for rel in files:
        f = Path(root) / rel
        if f.exists() and manifest.get(rel) == sha(f):
            os.utime(f, (FIXED_TIME, FIXED_TIME))
            kept += 1
    log('unchanged since last build:', kept, 'of', len(files))

def write_manifest(root, state):
    files = set()
    for part in ('branding', 'features'):
        st = state.get(part, {})
        for k in ('touched', 'added', 'both'):
            files.update(st.get(k, []))
    data = {rel: sha(Path(root) / rel) for rel in sorted(files) if (Path(root) / rel).exists()}
    mf = Path(root) / MANIFEST
    mf.parent.mkdir(parents=True, exist_ok=True)
    mf.write_text(json.dumps(data, indent=1))
    log('manifest:', len(data), 'files')

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('tdesktop')
    ap.add_argument('--part', required=True,
                    choices=['freeze', 'branding', 'features', 'revert-features', 'manifest'])
    a = ap.parse_args()
    root = Path(a.tdesktop)
    state_file = root / STATE
    state = json.loads(state_file.read_text()) if state_file.exists() else {}
    p = Patcher(root)
    if a.part == 'freeze':
        freeze(root)
        return
    if a.part == 'manifest':
        write_manifest(root, state)
        return
    if a.part == 'branding':
        apply_branding(p)
        state['branding'] = {'touched': sorted(p.touched), 'added': sorted(p.added)}
        stabilize(root, sorted(p.touched | p.added))
    elif a.part == 'features':
        branded = set(state.get('branding', {}).get('touched', []))
        try:
            apply_features(p)
        finally:
            # Saved even on failure, so a partial application can be undone.
            state['features'] = {'touched': sorted(p.touched - branded),
                                 'both': sorted(p.touched & branded),
                                 'added': sorted(p.added)}
            state_file.write_text(json.dumps(state, indent=1))
        stabilize(root, sorted(p.touched | p.added))
    else:
        f = state.get('features', {})
        revert(root, f.get('touched', []) + f.get('both', []), f.get('added', []))
        if f.get('both'):
            # Files shared with branding were reset to upstream: brand them again.
            apply_branding(Patcher(root))
        state.pop('features', None)
    state_file.write_text(json.dumps(state, indent=1))
    log('done', a.part)

if __name__ == '__main__':
    main()
