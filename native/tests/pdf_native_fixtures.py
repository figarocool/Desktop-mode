"""Generate independent PDF fixtures; no user documents or downloads required."""
import ctypes as c
from pathlib import Path
import struct
import zlib
import sys

out = Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/desktop-pdf-fixtures')
out.mkdir(parents=True, exist_ok=True)
lib = c.CDLL('libcairo.so.2')
for fn, result, args in [
    ('cairo_pdf_surface_create', c.c_void_p, [c.c_char_p,c.c_double,c.c_double]),
    ('cairo_create', c.c_void_p, [c.c_void_p]),
    ('cairo_set_source_rgb', None, [c.c_void_p,c.c_double,c.c_double,c.c_double]),
    ('cairo_rectangle', None, [c.c_void_p,c.c_double,c.c_double,c.c_double,c.c_double]),
    ('cairo_fill', None, [c.c_void_p]),
    ('cairo_move_to', None, [c.c_void_p,c.c_double,c.c_double]),
    ('cairo_set_font_size', None, [c.c_void_p,c.c_double]),
    ('cairo_show_text', None, [c.c_void_p,c.c_char_p]),
    ('cairo_destroy', None, [c.c_void_p]),
    ('cairo_surface_destroy', None, [c.c_void_p]),
]:
    f = getattr(lib, fn); f.restype = result; f.argtypes = args
surface = lib.cairo_pdf_surface_create(str(out / 'cairo-text.pdf').encode(),300,400)
ctx = lib.cairo_create(surface)
lib.cairo_set_source_rgb(ctx,1,0,0); lib.cairo_rectangle(ctx,20,200,120,70); lib.cairo_fill(ctx)
lib.cairo_set_source_rgb(ctx,0,0,0); lib.cairo_move_to(ctx,20,80); lib.cairo_set_font_size(ctx,18)
lib.cairo_show_text(ctx,'Desktop Mode: città è più'.encode())
lib.cairo_destroy(ctx); lib.cairo_surface_destroy(surface)

# Valid object stream plus cross-reference stream, with a Flate RGB image.
contents = b'1 0 0 rg 20 20 80 60 re f 0 g BT /F1 24 Tf 20 200 Td (Native PDF) Tj ET q 80 0 0 60 150 20 cm /I1 Do Q'
embedded = [
    (1,b'<< /Type /Catalog /Pages 2 0 R >>'),
    (2,b'<< /Type /Pages /Kids [3 0 R] /Count 1 /MediaBox [0 0 300 400] >>'),
    (3,b'<< /Type /Page /Parent 2 0 R /Resources << /Font << /F1 5 0 R >> /XObject << /I1 6 0 R >> >> /Contents 4 0 R >>'),
    (5,b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>'),
]
body = bytearray(); header = bytearray()
for number, value in embedded:
    header.extend(f'{number} {len(body)} '.encode()); body.extend(value+b'\n')
object_data = zlib.compress(header+body)
image_data = zlib.compress(bytes([0,255,0])*16)
stream_data = zlib.compress(contents)
def stream(metadata, data):
    return b'<< '+metadata+f' /Length {len(data)} >>\nstream\n'.encode()+data+b'\nendstream'
def write_object_stream_image(name, image, filter_name, parameters=b''):
    objects = {
        4:stream(b'/Filter /FlateDecode',stream_data),
        6:stream(b'/Type /XObject /Subtype /Image /Width 4 /Height 4 /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /'+filter_name+parameters,image),
        7:stream(f'/Type /ObjStm /N 4 /First {len(header)} /Filter /FlateDecode'.encode(),object_data),
    }
    document = bytearray(b'%PDF-1.5\n%\xff\xff\xff\xff\n'); offsets = {}
    for number,value in objects.items():
        offsets[number] = len(document); document.extend(f'{number} 0 obj\n'.encode()+value+b'\nendobj\n')
    offsets[8] = len(document)
    xref = bytearray()
    for number in range(9):
        if number == 0: entry = (0,0,65535)
        elif number in offsets: entry = (1,offsets[number],0)
        else: entry = (2,7,next(i for i,(n,_) in enumerate(embedded) if n == number))
        xref.extend(struct.pack('>BIH',*entry))
    document.extend(b'8 0 obj\n'+stream(b'/Type /XRef /Size 9 /W [1 4 2] /Root 1 0 R',xref)+b'\nendobj\n')
    document.extend(f'startxref\n{offsets[8]}\n%%EOF\n'.encode())
    (out/name).write_bytes(document)
write_object_stream_image('object-stream-image.pdf',image_data,b'FlateDecode')
# PNG Sub predictor: first pixel green, remaining pixels delta zero.
predictor_data=zlib.compress((bytes([1,0,255,0])+bytes(9))*4)
write_object_stream_image('predictor-image.pdf',predictor_data,b'FlateDecode',b' /DecodeParms << /Predictor 15 /Columns 4 /Colors 3 /BitsPerComponent 8 >>')
try:
    from PIL import Image
    from io import BytesIO
    jpeg=BytesIO();Image.new('RGB',(4,4),(0,255,0)).save(jpeg,format='JPEG',quality=95)
    write_object_stream_image('jpeg-image.pdf',jpeg.getvalue(),b'DCTDecode')
except ImportError:
    print('Optional JPEG fixture requires Pillow')
print(out)
