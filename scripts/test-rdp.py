#!/usr/bin/env python3
"""Real FreeRDP 3 client over a memory transport: no socket or user files.

Requires Linux x86_64, libfreerdp3/libwinpr3 >= 3.18, cc and OpenSSL headers.
The documented FreeRDP 3 ALIGN64 fields are used only by this test adapter.
Production Desktop Mode has no dependency on FreeRDP, Python or this adapter.
"""
import argparse
import ctypes as C
import errno
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent


def client(library, case):
    os.environ.setdefault('WLOG_LEVEL', 'ERROR')
    assert C.sizeof(C.c_void_p) == 8, 'FreeRDP adapter requires a 64-bit host'
    lib = C.CDLL('libfreerdp3.so.3')
    ptr = C.c_void_p

    def fn(name, args, result=C.c_int):
        method = getattr(lib, name)
        method.argtypes, method.restype = args, result
        return method

    version = fn('freerdp_get_version_string', [], C.c_char_p)().decode()
    assert version.startswith('3.'), version
    instance = fn('freerdp_new', [], ptr)()
    assert fn('freerdp_context_new', [ptr])(instance)
    slots = (C.c_uint64 * 128).from_address(instance)
    context = slots[0]
    context_slots = (C.c_uint64 * 128).from_address(context)
    settings = context_slots[40]
    key = fn('freerdp_settings_get_key_for_name', [C.c_char_p], C.c_ssize_t)
    set_string = fn('freerdp_settings_set_string', [ptr, C.c_ssize_t, C.c_char_p])
    set_bool = fn('freerdp_settings_set_bool', [ptr, C.c_ssize_t, C.c_int])
    set_int = fn('freerdp_settings_set_uint32', [ptr, C.c_ssize_t, C.c_uint32])
    for name, value in {'ServerHostname': 'memory-test',
                        'Username': 'wrong' if case == 'wrong-user' else 'desktop',
                        'Password': 'wrong-password' if case == 'wrong-password' else 'test-rdp-password'}.items():
        assert set_string(settings, key(('FreeRDP_' + name).encode()), value.encode()), name
    for name, value in {'ServerPort': 33989, 'DesktopWidth': 960, 'DesktopHeight': 544,
                        'ColorDepth': 32 if case == 'depth32' else 16,
                        'TcpConnectTimeout': 5000}.items():
        assert set_int(settings, key(('FreeRDP_' + name).encode()), value), name
    for name, value in {'NlaSecurity': 0, 'TlsSecurity': 1, 'RdpSecurity': 0,
                        'IgnoreCertificate': 1, 'AutoLogonEnabled': 1,
                        'FastPathInput': 0, 'FastPathOutput': 0,
                        'SupportGraphicsPipeline': 0, 'SupportDisplayControl': 0,
                        'RedirectClipboard': 0}.items():
        # Certificate bypass belongs only to this isolated in-memory test.
        assert set_bool(settings, key(('FreeRDP_' + name).encode()), value), name
    frame = (C.c_ubyte * (960 * 544 * 4))()
    gdi_init = fn('gdi_init_ex', [ptr, C.c_uint32, C.c_uint32, ptr, ptr])
    pixel_format = fn('FreeRDPGetColorFromatFromName', [C.c_char_p], C.c_uint32)(b'PIXEL_FORMAT_BGRA32')
    ConnectCallback = C.CFUNCTYPE(C.c_int, ptr)

    @ConnectCallback
    def post_connect(current):
        return gdi_init(current, pixel_format, 960 * 4, frame, None)

    slots[49] = C.cast(post_connect, ptr).value
    core = C.CDLL(library)
    core.dm_rdp_test_begin.argtypes = [C.c_char_p]
    core.dm_rdp_test_feed.argtypes = [ptr, C.c_int]
    core.dm_rdp_test_read.argtypes = [ptr, C.c_int]
    assert core.dm_rdp_test_begin(b'test-rdp-password') == 0
    assert core.dm_rdp_test_fill() == 0
    get_io = fn('freerdp_get_io_callbacks', [ptr], ptr)
    set_io = fn('freerdp_set_io_callbacks', [ptr, ptr])
    new_layer = fn('transport_layer_new', [ptr, C.c_size_t], ptr)
    ReadWrite = C.CFUNCTYPE(C.c_int, ptr, ptr, C.c_int, use_errno=True)
    Close = C.CFUNCTYPE(C.c_int, ptr)
    Wait = C.CFUNCTYPE(C.c_int, ptr, C.c_int, C.c_uint32)
    GetEvent = C.CFUNCTYPE(ptr, ptr)
    ConnectLayer = C.CFUNCTYPE(ptr, ptr, C.c_char_p, C.c_int, C.c_uint32)
    closing = False

    @ReadWrite
    def read_memory(_user, data, length):
        count = core.dm_rdp_test_read(data, length)
        if count <= 0:
            C.set_errno(errno.EAGAIN if count == 0 else errno.EIO)
            return -1
        return count

    @ReadWrite
    def write_memory(_user, data, length):
        if core.dm_rdp_test_feed(data, length) < 0 and not closing:
            C.set_errno(errno.EIO)
            return -1
        return length

    @Close
    def close_memory(_user):
        return 1

    @Wait
    def wait_memory(_user, _write, _timeout):
        return 1

    winpr = C.CDLL('libwinpr3.so.3')
    winpr.CreateEventW.argtypes = [ptr, C.c_int, C.c_int, ptr]
    winpr.CreateEventW.restype = ptr
    event_handle = winpr.CreateEventW(None, 1, 1, None)
    assert event_handle

    @GetEvent
    def get_event(_user):
        return event_handle

    @ConnectLayer
    def connect_memory(transport, _hostname, _port, _timeout):
        layer = new_layer(transport, 0)
        fields = (C.c_uint64 * 64).from_address(layer)
        for index, callback in [(1, read_memory), (2, write_memory), (3, close_memory),
                                (4, wait_memory), (5, get_event)]:
            fields[index] = C.cast(callback, ptr).value
        return layer

    io = (C.c_uint64 * 64)()
    C.memmove(io, get_io(context), C.sizeof(io))
    io[10] = C.cast(connect_memory, ptr).value
    assert set_io(context, C.addressof(io))
    connected = fn('freerdp_connect', [ptr])(instance)
    if case.startswith('wrong-'):
        assert not connected and core.dm_rdp_test_phase() == 4, 'Authentication unexpectedly accepted'
        print(f'PASS: FreeRDP {version}: rejects {case}', flush=True)
        return
    assert connected and core.dm_rdp_test_phase() == 7
    check = fn('freerdp_check_event_handles', [ptr])
    for _ in range(500):
        assert check(context)
        time.sleep(.001)
    for x, y, expected in [(10, 10, (0, 0, 255)), (950, 10, (255, 0, 0)),
                           (10, 530, (0, 255, 255)), (950, 530, (255, 255, 0))]:
        offset = (y * 960 + x) * 4
        assert tuple(frame[offset:offset + 3]) == expected, (x, y)
    mouse = fn('freerdp_input_send_mouse_event', [ptr, C.c_uint16, C.c_uint16, C.c_uint16])
    unicode_key = fn('freerdp_input_send_unicode_keyboard_event', [ptr, C.c_uint16, C.c_uint16])
    keyboard = fn('freerdp_input_send_keyboard_event', [ptr, C.c_uint16, C.c_uint16])
    input_handle = context_slots[38]
    assert mouse(input_handle, 0x9000, 150, 120)
    assert mouse(input_handle, 0x0800, 190, 140)
    assert mouse(input_handle, 0x1000, 190, 140)
    assert mouse(input_handle, 0xa000, 190, 140)
    assert mouse(input_handle, 0x2000, 190, 140)
    assert unicode_key(input_handle, 0, ord('é'))
    assert keyboard(input_handle, 0, 0x1c)
    assert keyboard(input_handle, 0, 0x1d)  # Ctrl down, click, Ctrl up in one batch.
    assert mouse(input_handle, 0x9000, 200, 150)
    assert mouse(input_handle, 0x1000, 200, 150)
    assert keyboard(input_handle, 0x8000, 0x1d)

    class Event(C.Structure):
        _fields_ = [(name, C.c_int) for name in ['kind', 'x', 'y', 'left', 'right', 'key', 'modifiers']] + [('text', C.c_char * 8)]

    core.dm_rdp_event.argtypes = [C.POINTER(Event)]
    received, event = [], Event()
    while core.dm_rdp_event(C.byref(event)):
        received.append((event.kind, event.x, event.y, event.left, event.right,
                         event.key, event.modifiers, event.text))
    assert any(e[1:4] == (150, 120, 1) for e in received)
    assert any(e[1:4] == (190, 140, 1) for e in received)
    assert any(e[1:4] == (190, 140, 0) for e in received)
    assert any(e[4] == 1 for e in received)
    assert any(e[7] == 'é'.encode() for e in received)
    assert any(e[5] == 2 for e in received)
    assert any(e[1] == 200 and e[6] == 1 for e in received)
    closing = True
    fn('freerdp_disconnect', [ptr])(instance)
    fn('gdi_free', [ptr], None)(instance)
    fn('freerdp_context_free', [ptr], None)(instance)
    fn('freerdp_free', [ptr], None)(instance)
    core.dm_rdp_stop()
    print(f'PASS: FreeRDP {version}: TLS 1.2 login, {case}, decoded bitmap pixels, mouse/drag/right-click, UTF-8, Enter and Ctrl-click', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library')
    parser.add_argument('--case', choices=['valid', 'depth32', 'wrong-password', 'wrong-user'], default='valid')
    args = parser.parse_args()
    if args.library:
        client(args.library, args.case)
        return
    with tempfile.TemporaryDirectory(prefix='desktop-rdp-test-') as directory:
        library = str(Path(directory) / 'rdp-test.so')
        subprocess.run(['cc', '-DDESKTOP_PREVIEW', '-DDM_RDP_TEST', '-std=gnu11',
                        '-Wall', '-Wextra', '-Werror', '-O1', '-g', '-fPIC', '-shared',
                        '-Inative/desktop/include', 'native/rdp_server.c', 'native/tests/rdp_stubs.c',
                        '-lssl', '-lcrypto', '-o', library], cwd=ROOT, check=True)
        for case in ['valid', 'depth32', 'wrong-password', 'wrong-user']:
            subprocess.run([sys.executable, str(Path(__file__).resolve()), '--library', library,
                            '--case', case], cwd=ROOT, check=True, timeout=20)


if __name__ == '__main__':
    main()
