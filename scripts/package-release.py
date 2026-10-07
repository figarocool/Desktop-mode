#!/usr/bin/env python3
"""Package already-built native artifacts without touching preview user data."""
from pathlib import Path
import shutil
import zipfile

root = Path(__file__).resolve().parent.parent
build = root / 'native/build'
apps = ['notepad', 'counter', 'browser', 'calculator', 'taskmanager', 'paint', 'images', 'console', 'pdf', 'network', 'solitaire', 'minesweeper', 'media']


def archive(name):
    return zipfile.ZipFile(build / name, 'w', zipfile.ZIP_DEFLATED)


with archive('desktop-mode-apps.zip') as z:
    for app in apps:
        package = build / f'{app}.dmapp'
        z.write(package, f'apps/{app}.dmapp')
    z.write(root / 'docs/pdf-viewer.md', 'PDF-LIMITI.md')

with archive('desktop-mode-sdk.zip') as z:
    z.write(root / 'native/desktop_api.h', 'desktop-mode-sdk/desktop_api.h')
    for directory in ['app-sdk', 'apps', 'pdf', 'licenses']:
        source = root / 'native' / directory
        for path in source.rglob('*'):
            if path.is_file() and '__pycache__' not in path.parts:
                z.write(path, 'desktop-mode-sdk/' + directory + '/' + str(path.relative_to(source)))
    for path in (root / 'docs').glob('*.md'):
        z.write(path, 'desktop-mode-sdk/docs/' + path.name)
    z.write(root / 'docs/desktop-mode.rdp', 'desktop-mode-sdk/docs/desktop-mode.rdp')
    z.write(root / 'docs/desktop-api.md', 'desktop-mode-sdk/README.md')
    z.write(root / 'native/assets/pdf-font.ttf', 'desktop-mode-sdk/assets/pdf-font.ttf')

with archive('desktop-mode-source.zip') as z:
    for directory in ['native', 'scripts', 'docs']:
        for path in (root / directory).rglob('*'):
            relative = path.relative_to(root)
            if path.is_file() and not any(p in ['build', 'demo', '__pycache__', '.git'] for p in relative.parts):
                z.write(path, 'desktop-mode/' + str(relative))
    z.write(root / 'README.md', 'desktop-mode/README.md')
    for path in (build / 'CMakeFiles/desktop_mode.dir').glob('*.obj'):
        z.write(path, 'desktop-mode/relink/' + path.name)
    z.write(build / 'CMakeFiles/desktop_mode.dir/link.txt', 'desktop-mode/relink/link.txt')
    z.write(build / 'libdesktop_pdf.a', 'desktop-mode/relink/libdesktop_pdf.a')

with archive('desktop-pdf-library.zip') as z:
    for directory in ['pdf', 'licenses']:
        for path in (root / 'native' / directory).glob('*'):
            z.write(path, 'desktop-pdf/native/' + directory + '/' + path.name)
    for source, target in [
        ('native/apps/pdf_engine.h', 'native/apps/pdf_engine.h'),
        ('native/assets/pdf-font.ttf', 'native/assets/pdf-font.ttf'),
        ('scripts/build-pdf-library.sh', 'scripts/build-pdf-library.sh'),
        ('docs/pdf-viewer.md', 'README.md'),
    ]:
        z.write(root / source, 'desktop-pdf/' + target)
    for platform in ['linux', 'vita']:
        z.write(build / f'pdf-library-{platform}/libdesktop_pdf.a', f'desktop-pdf/lib/{platform}/libdesktop_pdf.a')

shutil.copyfile(build / 'apps/pdf.dmapp', build / 'pdf-linux.dmapp')
with zipfile.ZipFile(build / 'desktop-mode.vpk') as z:
    for app in apps:
        assert z.read(f'apps/{app}.dmapp') == (build / f'{app}.dmapp').read_bytes()
    assert z.read('assets/pdf-font.ttf') == (root / 'native/assets/pdf-font.ttf').read_bytes()
for name in ['desktop-mode.vpk', 'desktop-mode-apps.zip', 'desktop-mode-sdk.zip', 'desktop-mode-source.zip', 'desktop-pdf-library.zip']:
    with zipfile.ZipFile(build / name) as z:
        assert z.testzip() is None
    print(name, (build / name).stat().st_size)
print('PASS: release archives and VPK app packages verified')
