#!/usr/bin/env python3
"""Desktop Mode app envelope: DMAPP001, platform, API, payload, kind, ABI."""
import struct
import sys
from pathlib import Path
source, platform, destination = sys.argv[1:]
payload = Path(source).read_bytes()
if not 0 < len(payload) <= 32 * 1024 * 1024:
    raise SystemExit('Modulo vuoto o superiore a 32 MiB')
if platform == 'wasm':
    platform_id, kind, app_abi = 0, 1, 1
else:
    platform_id, kind, app_abi = {'vita': 1, 'linux': 2}[platform], 0, 0
Path(destination).write_bytes(struct.pack('<8sIIQII', b'DMSAV001' if Path(destination).suffix == '.dmsaver' else b'DMAPP001', platform_id, 3, len(payload), kind, app_abi) + payload)
