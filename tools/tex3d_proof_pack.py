# A proof pack for the 3D texture replacement (src/video/tex3d_hd.h), made from a dump of the game's own
# textures: every image is scaled up and has its red and blue channels swapped, so that what is replaced on the
# screen can be seen at a glance without anything being drawn by hand. Nothing it makes belongs in the
# repository -- it is the game's own pictures, altered.
#
#   py -3 tools/tex3d_proof_pack.py <dump folder> <pack folder> [--scale N] [--only WxH]
#
# The file names are the keys, and are kept exactly as the dump wrote them: <hash>_<w>x<h>_f<format>.png, where
# the size is the texture's native size and the image's own size is free. No PNG library: the dump writes
# 8-bit RGBA with every row unfiltered, and that is what this reads and writes.
import os, sys, zlib, struct


def read_png(path):
    d = open(path, 'rb').read()
    assert d[:8] == b'\x89PNG\r\n\x1a\n', path
    at, idat, w, h, chans = 8, b'', 0, 0, 0
    while at + 12 <= len(d):
        n = struct.unpack('>I', d[at:at + 4])[0]
        kind = d[at + 4:at + 8]
        body = d[at + 8:at + 8 + n]
        if kind == b'IHDR':
            w, h, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', body)
            assert depth == 8 and interlace == 0, path
            chans = {0: 1, 2: 3, 4: 2, 6: 4}[colour]
        elif kind == b'IDAT':
            idat += body
        elif kind == b'IEND':
            break
        at += 12 + n
    raw = zlib.decompress(idat)
    stride = w * chans
    rows = []
    prev = bytearray(stride)
    for y in range(h):
        ft = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - chans] if i >= chans else 0
            b = prev[i]
            c = prev[i - chans] if i >= chans else 0
            if ft == 1:
                line[i] = (line[i] + a) & 255
            elif ft == 2:
                line[i] = (line[i] + b) & 255
            elif ft == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif ft == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
            elif ft != 0:
                raise SystemExit('%s: filter %d' % (path, ft))
        rows.append(line)
        prev = line
    px = bytearray(w * h * 4)
    for y in range(h):
        line = rows[y]
        for x in range(w):
            s = line[x * chans:(x + 1) * chans]
            o = (y * w + x) * 4
            if chans == 1:
                px[o:o + 4] = bytes([s[0], s[0], s[0], 255])
            elif chans == 2:
                px[o:o + 4] = bytes([s[0], s[0], s[0], s[1]])
            elif chans == 3:
                px[o:o + 4] = bytes([s[0], s[1], s[2], 255])
            else:
                px[o:o + 4] = s
    return w, h, px


def write_png(path, w, h, px):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += px[y * w * 4:(y + 1) * w * 4]
    out = [b'\x89PNG\r\n\x1a\n']
    for kind, body in ((b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)),
                       (b'IDAT', zlib.compress(bytes(raw), 6)), (b'IEND', b'')):
        out.append(struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body)))
    open(path, 'wb').write(b''.join(out))


def main():
    args = [a for a in sys.argv[1:]]
    scale, only = 2, None
    if '--scale' in args:
        i = args.index('--scale')
        scale = int(args[i + 1])
        del args[i:i + 2]
    if '--only' in args:
        i = args.index('--only')
        only = args[i + 1]
        del args[i:i + 2]
    if len(args) != 2:
        raise SystemExit(__doc__ or 'usage: tex3d_proof_pack.py <dump folder> <pack folder> [--scale N] [--only WxH]')
    src, dst = args
    os.makedirs(dst, exist_ok=True)
    made = 0
    for name in sorted(os.listdir(src)):
        if not name.endswith('.png'):
            continue
        if only and ('_%s_' % only) not in name:
            continue
        w, h, px = read_png(os.path.join(src, name))
        nw, nh = w * scale, h * scale
        out = bytearray(nw * nh * 4)
        for y in range(nh):
            sy = y // scale
            for x in range(nw):
                o, i = (y * nw + x) * 4, (sy * w + x // scale) * 4
                out[o] = px[i + 2]        # red and blue swapped: the replacement is unmistakable
                out[o + 1] = px[i + 1]
                out[o + 2] = px[i]
                out[o + 3] = px[i + 3]
        write_png(os.path.join(dst, name), nw, nh, out)
        made += 1
    print('%d images written to %s (x%d, red and blue swapped)' % (made, dst, scale))


main()
