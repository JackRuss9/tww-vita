#!/usr/bin/env python3
"""Dump an event from a stage's event_list.dat on your own GZLE01 disc image.

    python3 -I tools/disc_events.py tww/orig/GZLE01/GZLE01.ciso MajyuE FIRST_HINT
    python3 -I tools/disc_events.py <image> <stage> --list

Reads CISO images (the format this project links), the disc's FST, Yaz0 and
RARC archives. Prints each staff of the event with its cuts, start flags and
data (the arguments the actors and the camera read). No game data is stored.
"""
import struct
import sys


class Disc:
    def __init__(self, path):
        self.f = open(path, 'rb')
        hdr = self.f.read(0x8000)
        if hdr[:4] != b'CISO':
            raise SystemExit('only CISO images are supported')
        self.bs = struct.unpack('<I', hdr[4:8])[0]
        self.idx = []
        n = 0
        for b in hdr[8:]:
            self.idx.append(n if b else -1)
            n += b
        h = self.read(0, 0x440)
        fo, fs = struct.unpack('>II', h[0x424:0x42c])
        fst = self.read(fo, fs)
        count = struct.unpack('>I', fst[8:12])[0]
        strs = fst[count * 12:]

        def name(o):
            return strs[o:strs.index(b'\0', o)].decode()

        self.files = {}
        ends = [(count, '')]
        for i in range(1, count):
            w, a, b = struct.unpack('>III', fst[i * 12:i * 12 + 12])
            while ends and i >= ends[-1][0]:
                ends.pop()
            base = ends[-1][1] if ends else ''
            if w >> 24:
                ends.append((b, base + name(w & 0xffffff) + '/'))
            else:
                self.files[base + name(w & 0xffffff)] = (a, b)

    def read(self, off, size):
        out = bytearray()
        while size > 0:
            blk, o = divmod(off, self.bs)
            k = min(size, self.bs - o)
            if self.idx[blk] < 0:
                out += bytes(k)
            else:
                self.f.seek(0x8000 + self.idx[blk] * self.bs + o)
                out += self.f.read(k)
            off += k
            size -= k
        return bytes(out)

    def file(self, path):
        a, b = self.files[path]
        return self.read(a, b)


def yaz0(d):
    if d[:4] != b'Yaz0':
        return d
    size = struct.unpack('>I', d[4:8])[0]
    out = bytearray()
    p = 16
    while len(out) < size:
        c = d[p]
        p += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if c & (0x80 >> bit):
                out.append(d[p])
                p += 1
            else:
                b1, b2 = d[p], d[p + 1]
                p += 2
                dist = ((b1 & 0xf) << 8 | b2) + 1
                k = b1 >> 4
                if k == 0:
                    k = d[p] + 0x12
                    p += 1
                else:
                    k += 2
                for _ in range(k):
                    out.append(out[-dist])
    return bytes(out)


def rarc(d):
    doff = struct.unpack('>I', d[0xC:0x10])[0] + 0x20
    nfe = struct.unpack('>I', d[0x28:0x2C])[0]
    feo = struct.unpack('>I', d[0x2C:0x30])[0] + 0x20
    so = struct.unpack('>I', d[0x34:0x38])[0] + 0x20
    out = {}
    for k in range(nfe):
        fid, _h, typ, _p, no, off, sz = struct.unpack('>HHBBHII4x', d[feo + k * 20:feo + k * 20 + 20])
        if fid == 0xffff or typ & 2:
            continue
        out[d[so + no:d.index(b'\0', so + no)].decode()] = d[doff + off:doff + off + sz]
    return out


def cstr(b):
    return b.split(b'\0')[0].decode(errors='replace')


def main():
    disc = Disc(sys.argv[1])
    stage = sys.argv[2]
    d = rarc(yaz0(disc.file('res/Stage/%s/Stage.arc' % stage)))['event_list.dat']
    et, en, st, sn, ct, cn, dt, dn, ft, fn, it, inn, sdt, sdn = struct.unpack('>14I', d[:56])
    for e in range(en):
        ev = d[et + e * 0xB0:et + e * 0xB0 + 0xB0]
        name = cstr(ev[:32])
        if sys.argv[3] == '--list':
            print(e, name)
            continue
        if name != sys.argv[3]:
            continue
        ns = struct.unpack('>i', ev[0x7C:0x80])[0]
        staffs = struct.unpack('>20I', ev[0x2C:0x7C])[:ns]
        print('event %d %s start-flags %s finish-flags %s' % (
            e, name, struct.unpack('>2i', ev[0x80:0x88]), struct.unpack('>3i', ev[0x88:0x94])))
        for si in staffs:
            s = d[st + si * 0x50:st + si * 0x50 + 0x50]
            print(' staff %d %s type %d' % (si, cstr(s[:32]), struct.unpack('>i', s[0x2C:0x30])[0]))
            c = struct.unpack('>i', s[0x30:0x34])[0]
            cuts = set()
            while 0 <= c < cn and c not in cuts:
                cuts.add(c)
                cut = d[ct + c * 0x50:ct + c * 0x50 + 0x50]
                start = struct.unpack('>3i', cut[0x28:0x34])
                flag, first, nxt = struct.unpack('>iii', cut[0x34:0x40])
                data = []
                di = first
                seen = set()
                while 0 <= di < dn and di not in seen:
                    seen.add(di)
                    dd = d[dt + di * 0x40:dt + di * 0x40 + 0x40]
                    typ, sub, num, nx = struct.unpack('>iiii', dd[0x24:0x34])
                    if typ in (0, 1):
                        cnt = num * (3 if typ == 1 else 1)
                        v = [round(x, 2) for x in struct.unpack('>%df' % cnt, d[ft + sub * 4:ft + sub * 4 + cnt * 4])]
                    elif typ == 3:
                        v = list(struct.unpack('>%di' % num, d[it + sub * 4:it + sub * 4 + num * 4]))
                    elif typ == 4:
                        v = cstr(d[sdt + sub:sdt + sub + num])
                    else:
                        v = 'type%d' % typ
                    data.append('%s=%s' % (cstr(dd[:32]), v))
                    di = nx
                print('   cut %d %s start %s flag %d %s' % (c, cstr(cut[:32]), start, flag, ' '.join(data)))
                c = nxt


if __name__ == '__main__':
    main()
